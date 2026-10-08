#include "BorealAnalyzer.h"

#include "../Helpers/LorisStdCompat.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "AiffFile.h"
#include "Analyzer.h"
#include "Channelizer.h"
#include "Distiller.h"
#include "Fundamental.h"
#include "LinearEnvelope.h"
#include "Marker.h"
#include "SdifFile.h"
#include "Synthesizer.h"

namespace boreal
{
    namespace analyzer
    {
        const char* stageName (Stage s)
        {
            switch (s)
            {
                case Stage::Reading:          return "reading";
                case Stage::DetectingOnsets:  return "detecting onsets";
                case Stage::TrackingPartials: return "tracking partials";
                case Stage::Labeling:         return "labeling";
                case Stage::Writing:          return "writing";
                case Stage::Done:             break;
            }
            return "done";
        }

        namespace
        {
            uint32_t readU32LE (const std::vector<char>& b, size_t o)
            {
                return (uint32_t) (uint8_t) b[o]
                     | ((uint32_t) (uint8_t) b[o + 1] << 8)
                     | ((uint32_t) (uint8_t) b[o + 2] << 16)
                     | ((uint32_t) (uint8_t) b[o + 3] << 24);
            }

            uint16_t readU16LE (const std::vector<char>& b, size_t o)
            {
                return (uint16_t) ((uint16_t) (uint8_t) b[o] | ((uint16_t) (uint8_t) b[o + 1] << 8));
            }

            int32_t readI24LE (const std::vector<char>& b, size_t o)
            {
                const uint32_t v = (uint32_t) (uint8_t) b[o]
                                 | ((uint32_t) (uint8_t) b[o + 1] << 8)
                                 | ((uint32_t) (uint8_t) b[o + 2] << 16);
                const uint32_t sign = v & 0x800000u;
                return (int32_t) ((v ^ sign) - sign);
            }

            uint32_t readU32BE (const std::vector<char>& b, size_t o)
            {
                return ((uint32_t) (uint8_t) b[o] << 24)
                     | ((uint32_t) (uint8_t) b[o + 1] << 16)
                     | ((uint32_t) (uint8_t) b[o + 2] << 8)
                     | (uint32_t) (uint8_t) b[o + 3];
            }

            uint16_t readU16BE (const std::vector<char>& b, size_t o)
            {
                return (uint16_t) ((uint16_t) (uint8_t) b[o] << 8 | (uint16_t) (uint8_t) b[o + 1]);
            }

            int32_t readI24BE (const std::vector<char>& b, size_t o)
            {
                const uint32_t v = ((uint32_t) (uint8_t) b[o] << 16)
                                 | ((uint32_t) (uint8_t) b[o + 1] << 8)
                                 | (uint32_t) (uint8_t) b[o + 2];
                const uint32_t sign = v & 0x800000u;
                return (int32_t) ((v ^ sign) - sign);
            }

            float readF32LE (const std::vector<char>& b, size_t o)
            {
                const uint32_t bits = readU32LE (b, o);
                float f;
                std::memcpy (&f, &bits, 4);
                return f;
            }

            double readF64LE (const std::vector<char>& b, size_t o)
            {
                const uint64_t bits = (uint64_t) readU32LE (b, o) | ((uint64_t) readU32LE (b, o + 4) << 32);
                double d;
                std::memcpy (&d, &bits, 8);
                return d;
            }
        }

        namespace
        {

            bool readWav (const std::string& path, std::vector<double>& mono, double& sampleRate, std::string& err)
            {
                std::ifstream in (path, std::ios::binary);
                if (! in)
                {
                    err = "cannot open file";
                    return false;
                }

                char fourcc[4];
                in.read (fourcc, 4);
                if (std::string (fourcc, 4) != "RIFF")
                {
                    err = "not a RIFF file";
                    return false;
                }
                in.seekg (4, std::ios::cur);
                in.read (fourcc, 4);
                if (std::string (fourcc, 4) != "WAVE")
                {
                    err = "not a WAVE file";
                    return false;
                }
                const std::streamoff fileEnd = in.seekg (0, std::ios::end).tellg();
                in.seekg (12, std::ios::beg);

                uint16_t audioFormat = 1, channels = 0, bitsPerSample = 0;
                uint32_t fileSampleRate = 0;
                uint32_t dataOffset = 0, dataSize = 0;
                bool haveFmt = false;

                while (in.read (fourcc, 4))
                {
                    char sz[4];
                    if (! in.read (sz, 4))
                        break;
                    const uint32_t chunkSize = readU32LE (std::vector<char> (sz, sz + 4), 0);
                    const uint32_t total = chunkSize + (chunkSize & 1u);

                    const std::string id (fourcc, 4);

                    const std::streamoff here = in.tellg();
                    if ((std::streamoff) chunkSize > fileEnd - here)
                    {
                        err = "malformed '" + id + "' chunk size";
                        return false;
                    }

                    if (id == "fmt ")
                    {
                        std::vector<char> fmt (chunkSize);
                        if (chunkSize < 16 || ! in.read (fmt.data(), (std::streamsize) chunkSize))
                        {
                            err = "truncated fmt chunk";
                            return false;
                        }
                        audioFormat = readU16LE (fmt, 0);
                        if (audioFormat == 0xFFFE && chunkSize >= 26)
                            audioFormat = readU16LE (fmt, 24);
                        channels      = readU16LE (fmt, 2);
                        fileSampleRate = readU32LE (fmt, 4);
                        bitsPerSample = readU16LE (fmt, 14);
                        haveFmt = true;
                        in.seekg ((std::streamoff) (total - chunkSize), std::ios::cur);
                    }
                    else if (id == "data")
                    {
                        dataOffset = (uint32_t) in.tellg();
                        dataSize = chunkSize;
                        in.seekg ((std::streamoff) (total - chunkSize), std::ios::cur);
                        break;
                    }
                    else
                    {
                        in.seekg ((std::streamoff) total, std::ios::cur);
                    }
                }

                if (! haveFmt)
                {
                    err = "missing fmt chunk";
                    return false;
                }
                if (audioFormat != 1 && audioFormat != 3)
                {
                    err = "unsupported encoding (only PCM and IEEE float)";
                    return false;
                }
                if (channels == 0 || bitsPerSample % 8 != 0 || dataSize == 0)
                {
                    err = "malformed header";
                    return false;
                }

                const unsigned bytesPerSample = bitsPerSample / 8;
                const unsigned bytesPerFrame = bytesPerSample * channels;
                const size_t numFrames = dataSize / bytesPerFrame;
                if (numFrames == 0)
                {
                    err = "no audio data";
                    return false;
                }

                std::vector<char> data (dataSize);
                in.seekg ((std::streamoff) dataOffset, std::ios::beg);
                if (! in.read (data.data(), (std::streamsize) dataSize))
                {
                    err = "truncated data chunk";
                    return false;
                }

                mono.clear();
                mono.reserve (numFrames);
                for (size_t f = 0; f < numFrames; ++f)
                {
                    double acc = 0.0;
                    for (unsigned c = 0; c < channels; ++c)
                    {
                        const size_t o = f * bytesPerFrame + c * bytesPerSample;
                        double v = 0.0;
                        if (audioFormat == 1)
                        {
                            if (bitsPerSample == 16)      v = (int16_t) readU16LE (data, o) / 32768.0;
                            else if (bitsPerSample == 24) v = readI24LE (data, o) / 8388608.0;
                            else if (bitsPerSample == 32) v = (int32_t) readU32LE (data, o) / 2147483648.0;
                            else
                            {
                                err = "unsupported PCM bit depth";
                                return false;
                            }
                        }
                        else
                        {
                            if (bitsPerSample == 32)      v = readF32LE (data, o);
                            else if (bitsPerSample == 64) v = readF64LE (data, o);
                            else
                            {
                                err = "unsupported float bit depth";
                                return false;
                            }
                        }
                        acc += v;
                    }
                    mono.push_back (acc / channels);
                }

                sampleRate = (double) fileSampleRate;
                return true;
            }

            bool readAiff (const std::string& path, std::vector<double>& mono, double& sampleRate, std::string& err)
            {
                std::ifstream in (path, std::ios::binary);
                if (! in)
                {
                    err = "cannot open file";
                    return false;
                }

                char fourcc[4];
                in.read (fourcc, 4);
                if (std::string (fourcc, 4) != "FORM")
                {
                    err = "not an AIFF file";
                    return false;
                }
                in.seekg (4, std::ios::cur);
                in.read (fourcc, 4);
                if (std::string (fourcc, 4) != "AIFF")
                {
                    err = "not an AIFF file";
                    return false;
                }
                const std::streamoff fileEnd = in.seekg (0, std::ios::end).tellg();
                in.seekg (12, std::ios::beg);

                uint16_t channels = 0, sampleSize = 0;
                uint32_t numFrames = 0;
                uint32_t dataOffset = 0, dataSize = 0;
                bool haveComm = false;

                while (in.read (fourcc, 4))
                {
                    char sz[4];
                    if (! in.read (sz, 4))
                        break;
                    const uint32_t chunkSize = readU32BE (std::vector<char> (sz, sz + 4), 0);
                    const uint32_t total = chunkSize + (chunkSize & 1u);

                    const std::string id (fourcc, 4);
                    const std::streamoff here = in.tellg();
                    if ((std::streamoff) chunkSize > fileEnd - here)
                    {
                        err = "malformed '" + id + "' chunk size";
                        return false;
                    }

                    if (id == "COMM")
                    {
                        std::vector<char> comm (chunkSize);
                        if (chunkSize < 18 || ! in.read (comm.data(), (std::streamsize) chunkSize))
                        {
                            err = "truncated COMM chunk";
                            return false;
                        }
                        channels   = readU16BE (comm, 0);
                        numFrames  = readU32BE (comm, 2);
                        sampleSize = readU16BE (comm, 6);

                        unsigned char raw[10];
                        for (int i = 0; i < 10; ++i)
                            raw[i] = (unsigned char) comm[(size_t) (8 + i)];
                        const unsigned sign = raw[0] & 0x80u;
                        const unsigned exp = (((unsigned) raw[0] & 0x7fu) << 8) | (unsigned) raw[1];
                        uint64_t mant = 0;
                        for (int i = 0; i < 8; ++i)
                            mant = (mant << 8) | (uint64_t) raw[2 + i];
                        if (exp == 0 && mant == 0)
                        {
                            err = "COMM chunk has zero sample rate";
                            return false;
                        }
                        sampleRate = std::ldexp ((double) mant, (int) exp - 16383 - 63);
                        if (sign)
                            sampleRate = -sampleRate;

                        haveComm = true;
                        in.seekg ((std::streamoff) (total - chunkSize), std::ios::cur);
                    }
                    else if (id == "SSND")
                    {
                        std::vector<char> hdr (8);
                        if (chunkSize < 8 || ! in.read (hdr.data(), 8))
                        {
                            err = "truncated SSND chunk";
                            return false;
                        }
                        const uint32_t offset = readU32BE (hdr, 0);
                        const uint32_t payload = chunkSize - 8;
                        if (offset > payload)
                        {
                            err = "bad SSND offset";
                            return false;
                        }
                        dataOffset = (uint32_t) in.tellg() + offset;
                        dataSize = payload - offset;
                        break;
                    }
                    else
                    {
                        in.seekg ((std::streamoff) total, std::ios::cur);
                    }
                }

                if (! haveComm)
                {
                    err = "missing COMM chunk";
                    return false;
                }
                if (channels == 0 || sampleSize % 8 != 0 || dataSize == 0)
                {
                    err = "malformed header";
                    return false;
                }

                const unsigned bytesPerSample = sampleSize / 8;
                const unsigned bytesPerFrame = bytesPerSample * channels;
                if (numFrames == 0 || dataSize < bytesPerFrame)
                {
                    err = "no audio data";
                    return false;
                }

                std::vector<char> data (dataSize);
                in.seekg ((std::streamoff) dataOffset, std::ios::beg);
                if (! in.read (data.data(), (std::streamsize) dataSize))
                {
                    err = "truncated SSND data";
                    return false;
                }

                mono.clear();
                mono.reserve (numFrames);
                for (uint32_t f = 0; f < numFrames; ++f)
                {
                    double acc = 0.0;
                    for (unsigned c = 0; c < channels; ++c)
                    {
                        const size_t o = (size_t) f * bytesPerFrame + c * bytesPerSample;
                        double v = 0.0;
                        if (sampleSize == 8)       v = (int8_t) data[o] / 128.0;
                        else if (sampleSize == 16) v = (int16_t) readU16BE (data, o) / 32768.0;
                        else if (sampleSize == 24) v = readI24BE (data, o) / 8388608.0;
                        else if (sampleSize == 32) v = (int32_t) readU32BE (data, o) / 2147483648.0;
                        else
                        {
                            err = "unsupported AIFF bit depth";
                            return false;
                        }
                        acc += v;
                    }
                    mono.push_back (acc / channels);
                }

                return true;
            }

            constexpr double kPi = 3.14159265358979323846;

            void fftInPlace (std::vector<double>& re, std::vector<double>& im)
            {
                const size_t n = re.size();
                for (size_t i = 1, j = 0; i < n; ++i)
                {
                    size_t bit = n >> 1;
                    for (; j & bit; bit >>= 1)
                        j ^= bit;
                    j ^= bit;
                    if (i < j)
                    {
                        std::swap (re[i], re[j]);
                        std::swap (im[i], im[j]);
                    }
                }
                for (size_t len = 2; len <= n; len <<= 1)
                {
                    const double ang = -2.0 * kPi / (double) len;
                    const double wStepR = std::cos (ang), wStepI = std::sin (ang);
                    const size_t half = len >> 1;
                    for (size_t base = 0; base < n; base += len)
                    {
                        double wr = 1.0, wi = 0.0;
                        for (size_t k = 0; k < half; ++k)
                        {
                            const double ur = re[base + k], ui = im[base + k];
                            const double vr = re[base + k + half] * wr - im[base + k + half] * wi;
                            const double vi = re[base + k + half] * wi + im[base + k + half] * wr;
                            re[base + k] = ur + vr;
                            im[base + k] = ui + vi;
                            re[base + k + half] = ur - vr;
                            im[base + k + half] = ui - vi;
                            const double nwr = wr * wStepR - wi * wStepI;
                            wi = wr * wStepI + wi * wStepR;
                            wr = nwr;
                        }
                    }
                }
            }

            double refineOnsetTime (const std::vector<double>& samples, double sampleRate,
                                    double approxT)
            {
                const int winN = std::max (2, (int) std::lround (0.003 * sampleRate));
                const int hopN = std::max (1, (int) std::lround (0.0005 * sampleRate));
                const long center = (long) (approxT * sampleRate);
                const long span = (long) std::lround (0.025 * sampleRate);
                const long begin = std::max ((long) winN, center - span);
                const long end = std::min ((long) samples.size() - 1, center + span);

                auto rmsAt = [&] (long pos) -> double
                {
                    if (pos < 0 || pos + winN >= (long) samples.size()) return 0.0;
                    double acc = 0.0;
                    for (long i = 0; i < winN; ++i)
                        acc += samples[(size_t) (pos + i)] * samples[(size_t) (pos + i)];
                    return std::sqrt (acc / winN);
                };

                double bestRise = -1.0;
                long bestPos = center;
                double prev = rmsAt (begin);
                for (long pos = begin + hopN; pos <= end; pos += hopN)
                {
                    const double cur = rmsAt (pos);
                    const double rise = cur - prev;
                    if (rise > bestRise)
                    {
                        bestRise = rise;
                        bestPos = pos;
                    }
                    prev = cur;
                }
                return (double) bestPos / sampleRate;
            }
        }

        bool writeWavMono16 (const std::string& path, double sampleRate, const std::vector<double>& samples)
        {
            std::ofstream out (path, std::ios::binary);
            if (! out)
                return false;

            double peak = 0.0;
            for (double s : samples)
                peak = std::max (peak, std::abs (s));
            const double norm = (peak > 1.0) ? (0.99 / peak) : 1.0;

            const uint32_t numSamples = (uint32_t) samples.size();
            const uint32_t byteRate = (uint32_t) sampleRate * 2;
            const uint32_t dataSize = numSamples * 2;
            const uint32_t riffSize = 36 + dataSize;

            auto writeU32 = [&] (uint32_t v) { out.write (reinterpret_cast<const char*> (&v), 4); };
            auto writeU16 = [&] (uint16_t v) { out.write (reinterpret_cast<const char*> (&v), 2); };

            out.write ("RIFF", 4);
            writeU32 (riffSize);
            out.write ("WAVE", 4);
            out.write ("fmt ", 4);
            writeU32 (16);
            writeU16 (1);
            writeU16 (1);
            writeU32 ((uint32_t) sampleRate);
            writeU32 (byteRate);
            writeU16 (2);
            writeU16 (16);
            out.write ("data", 4);
            writeU32 (dataSize);

            for (double s : samples)
            {
                const double clamped = std::max (-1.0, std::min (1.0, s * norm));
                const int16_t v = (int16_t) std::lround (clamped * 32767.0);
                out.write (reinterpret_cast<const char*> (&v), 2);
            }

            return true;
        }

        bool readAudioFile (const std::string& path, std::vector<double>& mono,
                            double& sampleRate, std::string& err)
        {
            std::string ext = path.substr (path.find_last_of ('.'));
            std::transform (ext.begin(), ext.end(), ext.begin(), [] (char c) { return (char) std::tolower (c); });

            if (ext == ".wav" || ext == ".wave")
                return readWav (path, mono, sampleRate, err);

            return readAiff (path, mono, sampleRate, err);
        }

        std::vector<double> detectOnsets (const std::vector<double>& samples, double sampleRate,
                                          double thresholdMult)
        {
            constexpr int fftSize = 1024;
            if ((int) samples.size() < fftSize * 2)
                return {};

            const int hop = std::max (1, (int) std::lround (0.003 * sampleRate));
            std::vector<double> window ((size_t) fftSize);
            for (int i = 0; i < fftSize; ++i)
                window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) (fftSize - 1));

            std::vector<double> flux;
            std::vector<double> re ((size_t) fftSize), im ((size_t) fftSize), prevMag ((size_t) fftSize / 2, 0.0);

            for (int start = 0; start + fftSize <= (int) samples.size(); start += hop)
            {
                for (int i = 0; i < fftSize; ++i)
                {
                    re[(size_t) i] = samples[(size_t) start + (size_t) i] * window[(size_t) i];
                    im[(size_t) i] = 0.0;
                }
                fftInPlace (re, im);
                double f = 0.0;
                for (int k = 1; k < fftSize / 2; ++k)
                {
                    const double mag = std::sqrt (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]);
                    if (mag > prevMag[(size_t) k])
                        f += mag - prevMag[(size_t) k];
                    prevMag[(size_t) k] = mag;
                }
                flux.push_back (f);
            }

            const int F = (int) flux.size();
            if (F < 5)
                return {};

            const double frameDur = (double) hop / sampleRate;
            const int medWin = std::max (2, (int) std::lround (0.15 / frameDur));
            const int halfLocal = 3;
            const double minGapSec = 0.03;

            std::vector<double> onsets;
            std::vector<double> neigh;
            neigh.reserve ((size_t) (2 * medWin + 1));
            double lastAccepted = -1.0e9;

            for (int fIdx = halfLocal; fIdx < F - halfLocal; ++fIdx)
            {
                neigh.clear();
                for (int j = std::max (0, fIdx - medWin); j <= std::min (F - 1, fIdx + medWin); ++j)
                    neigh.push_back (flux[(size_t) j]);
                const size_t mid = neigh.size() / 2;
                std::nth_element (neigh.begin(), neigh.begin() + (std::ptrdiff_t) mid, neigh.end());
                const double thr = neigh[mid] * thresholdMult + 1.0e-12;

                bool isLocalMax = true;
                for (int j = fIdx - halfLocal; j <= fIdx + halfLocal; ++j)
                    if (flux[(size_t) j] > flux[(size_t) fIdx])
                    {
                        isLocalMax = false;
                        break;
                    }
                if (! isLocalMax || flux[(size_t) fIdx] <= thr)
                    continue;

                const double t = ((double) fIdx * (double) hop + 0.5 * (double) fftSize) / sampleRate;
                if (t - lastAccepted >= minGapSec)
                {
                    onsets.push_back (refineOnsetTime (samples, sampleRate, t));
                    lastAccepted = t;
                }
            }
            return onsets;
        }

        double measureOnsetDensityPerSecond (const std::vector<double>& samples, double sampleRate)
        {
            if (sampleRate <= 0.0 || samples.empty())
                return 0.0;
            const size_t headLen = std::min (samples.size(), (size_t) sampleRate);
            const std::vector<double> head (samples.begin(), samples.begin() + (std::ptrdiff_t) headLen);
            const std::vector<double> hits = detectOnsets (head, sampleRate, 1.6);
            const double seconds = (double) headLen / sampleRate;
            return (seconds > 0.0) ? (double) hits.size() / seconds : 0.0;
        }

        bool sdifHasOnsetMarkers (const std::string& sdifPath)
        {
            try
            {
                Loris::SdifFile f (sdifPath);
                for (const auto& m : f.markers())
                    if (m.name().rfind ("onset", 0) == 0)
                        return true;
            }
            catch (...)
            {
            }
            return false;
        }

        bool analyzeToFiles (const std::string& inputAudioPath,
                             const std::string& outputBaseNoExt,
                             const Settings& settings,
                             const ProgressFn& progress,
                             Result* outResult,
                             std::string*)
        {

            std::string error;
            auto fail = [&] (const std::string& m) { error = m; return false; };
            auto cancelled = [&] () { error = "cancelled"; return false; };

            auto report = [&] (Stage s, float overall) -> bool
            {
                if (! progress)
                    return true;
                return progress (s, overall);
            };

            Result result;
            Stats& stats = result.stats;

            try
            {
                if (settings.resolution <= 0.0)
                    return fail ("resolution must be positive");

                if (! report (Stage::Reading, 0.0f))
                    return cancelled();

                std::vector<double> samples;
                double sampleRate = 0.0;
                if (! readAudioFile (inputAudioPath, samples, sampleRate, error))
                    return fail ("failed to read audio '" + inputAudioPath + "': " + error);

                if (samples.empty())
                    return fail ("no samples in '" + inputAudioPath + "'");
                if (sampleRate <= 0.0)
                    return fail ("invalid sample rate from '" + inputAudioPath + "'");
                if (settings.sampleRateOverride > 0.0)
                    sampleRate = settings.sampleRateOverride;

                stats.sourceDurationSeconds = (double) samples.size() / sampleRate;
                stats.sourceSampleCount = samples.size();
                stats.sampleRate = sampleRate;

                if (settings.maxDurationSeconds > 0.0 && stats.sourceDurationSeconds > settings.maxDurationSeconds)
                    return fail ("source is " + std::to_string (stats.sourceDurationSeconds)
                                 + "s long -- over the "
                                 + std::to_string (settings.maxDurationSeconds) + "s analysis cap");

                double matResolution = settings.resolution;
                double matAmpFloor = settings.ampFloor;
                bool matCleanPartials = settings.cleanPartials;
                if (settings.autoMaterial)
                {
                    stats.autoMaterialDensity = measureOnsetDensityPerSecond (samples, sampleRate);
                    stats.autoResolvedPercussive = stats.autoMaterialDensity >= Settings::kAutoOnsetDensityThreshold;
                    if (stats.autoResolvedPercussive)
                    {
                        matResolution = Settings::kPercussiveResolutionHz;
                        matAmpFloor = Settings::kPercussiveAmpFloorDb;
                        matCleanPartials = true;
                    }
                    else
                    {
                        matResolution = Settings::kTonalResolutionHz;
                        matAmpFloor = Settings::kTonalAmpFloorDb;
                        matCleanPartials = false;
                    }
                }

                if (! report (Stage::DetectingOnsets, 0.05f))
                    return cancelled();

                if (settings.regionStart > 0.0 || settings.regionEnd < 1.0)
                {
                    const size_t total = samples.size();
                    const double lo = std::clamp (settings.regionStart, 0.0, 0.999);
                    const double hi = std::clamp (settings.regionEnd, lo + 1.0e-3, 1.0);
                    const size_t i0 = (size_t) std::floor (lo * (double) total);
                    const size_t i1 = std::min (total, (size_t) std::ceil (hi * (double) total));

                    std::vector<double> sliced (samples.begin() + (std::ptrdiff_t) i0,
                                                samples.begin() + (std::ptrdiff_t) i1);
                    const int fadeN = std::max (1, (int) std::lround (0.001 * sampleRate));
                    const int fadeLen = (int) std::min ((size_t) fadeN, sliced.size() / 2);
                    for (int i = 0; i < fadeLen; ++i)
                    {
                        const double g = (double) i / (double) fadeLen;
                        sliced[(size_t) i] *= g;
                        sliced[sliced.size() - 1 - (size_t) i] *= g;
                    }

                    samples = std::move (sliced);
                    stats.sourceDurationSeconds = (double) samples.size() / sampleRate;
                    stats.sourceSampleCount = samples.size();
                }

                const std::vector<double> onsetTimes = detectOnsets (samples, sampleRate, settings.onsetMult);
                stats.onsetCount = onsetTimes.size();
                result.onsets = onsetTimes;

                if (! report (Stage::TrackingPartials, 0.15f))
                    return cancelled();

                const double resolution     = matResolution;
                const double effWidth      = (settings.windowWidth > 0.0) ? settings.windowWidth : 2.0 * resolution;
                const double effAmpFloor   = (std::fabs (matAmpFloor) > 0.0) ? matAmpFloor          : -90.0;
                const double effFreqFloor  = (settings.freqFloor > 0.0)   ? settings.freqFloor  : resolution;
                const double effDrift      = (settings.freqDrift > 0.0)   ? settings.freqDrift  : 0.5 * resolution;
                const double effHop        = (settings.hopTime > 0.0)     ? settings.hopTime    : (effWidth > 0.0 ? 1.0 / effWidth : 0.0);
                const double effCrop       = (settings.cropTime > 0.0)    ? settings.cropTime   : effHop;
                const double effSidelobe   = (settings.sidelobe > 0.0)    ? settings.sidelobe   : 90.0;

                stats.effectiveWidthHz = effWidth;
                stats.effectiveAmpFloorDb = effAmpFloor;
                stats.effectiveFreqFloorHz = effFreqFloor;
                stats.effectiveDriftHz = effDrift;
                stats.effectiveHopSec = effHop;
                stats.effectiveCropSec = effCrop;
                stats.effectiveSidelobeDb = effSidelobe;

                Loris::Analyzer analyzer (resolution, effWidth);
                analyzer.setAmpFloor (effAmpFloor);
                analyzer.setFreqFloor (effFreqFloor);
                analyzer.setFreqDrift (effDrift);
                analyzer.setHopTime (effHop);
                analyzer.setCropTime (effCrop);
                analyzer.setSidelobeLevel (effSidelobe);
                analyzer.setPhaseCorrect (settings.phaseCorrect);

                if (settings.bandwidthMode == "none")
                    analyzer.storeNoBandwidth();
                else if (settings.bandwidthMode == "convergence")
                    analyzer.storeConvergenceBandwidth (settings.convergence > 0.0 ? settings.convergence : 0.1);
                else
                    analyzer.storeResidueBandwidth (settings.bwWidth > 0.0 ? settings.bwWidth : 2000.0);

                const bool useF0 = (settings.fmin > 0.0);
                if (useF0)
                    analyzer.buildFundamentalEnv (settings.fmin, (settings.fmax > settings.fmin) ? settings.fmax : 2.0 * settings.fmin);

                Loris::PartialList partials;
                if (settings.referenceHz > 0.0)
                {
                    stats.trackingDescription = "constant reference " + std::to_string (settings.referenceHz) + " Hz (single pass)";
                    Loris::LinearEnvelope ref (settings.referenceHz);
                    partials = analyzer.analyze (samples, sampleRate, ref);
                }
                else if (useF0)
                {
                    stats.trackingDescription = "estimated fundamental envelope (two-pass)";
                    Loris::PartialList pass1 = analyzer.analyze (samples, sampleRate);
                    const Loris::LinearEnvelope& f0 = analyzer.fundamentalEnv();
                    partials = analyzer.analyze (samples, sampleRate, f0);
                }
                else
                {
                    stats.trackingDescription = "Loris default flat 1.0 reference (single pass)";
                    partials = analyzer.analyze (samples, sampleRate);
                }

                double maxTime = 0.0, minFreq = 1.0e9, maxFreq = 0.0;
                size_t totalBreakpoints = 0;
                for (auto& p : partials)
                {
                    maxTime = std::max (maxTime, p.endTime());
                    totalBreakpoints += (size_t) std::distance (p.begin(), p.end());
                    for (auto it = p.begin(); it != p.end(); ++it)
                    {
                        minFreq = std::min (minFreq, it->frequency());
                        maxFreq = std::max (maxFreq, it->frequency());
                    }
                }

                stats.rawPartialCount = partials.size();
                stats.rawBreakpointCount = totalBreakpoints;
                stats.rawDurationSeconds = maxTime;
                if (! partials.empty())
                {
                    stats.rawMinFrequencyHz = minFreq;
                    stats.rawMaxFrequencyHz = maxFreq;
                }

                try
                {
                    const Loris::LinearEnvelope& f0 = analyzer.fundamentalEnv();
                    stats.f0StartHz = f0.valueAt (0.0);
                    stats.f0MidHz = f0.valueAt (maxTime / 2.0);
                    stats.f0EndHz = f0.valueAt (maxTime);
                    stats.hasTrackerF0 = true;
                }
                catch (...)
                {
                    stats.hasTrackerF0 = false;
                }

                if (matCleanPartials)
                {
                    if (! report (Stage::Labeling, 0.60f))
                        return cancelled();

                    const double plLower = (settings.plLower > 0.0) ? settings.plLower
                                         : (settings.fmin > 0.0) ? 0.9 * settings.fmin : 50.0;
                    const double plUpper = (settings.plUpper > 0.0) ? settings.plUpper
                                         : (settings.fmax > settings.fmin) ? 1.1 * settings.fmax
                                         : (settings.fmin > 0.0) ? 1.5 * settings.fmin : 5000.0;
                    stats.pipelineSearchLowHz = plLower;
                    stats.pipelineSearchHighHz = plUpper;

                    Loris::LinearEnvelope ref (220.0);
                    try
                    {
                        Loris::FundamentalFromPartials estimator;
                        ref = estimator.buildEnvelope (partials, 0.0, maxTime,
                                                       settings.plInterval, plLower, plUpper,
                                                       settings.plConfidence);
                    }
                    catch (const std::exception&)
                    {

                        stats.pipelineUsedConstantRef = true;
                    }

                    const size_t before = partials.size();

                    {
                        const int n = std::max (1, (int) std::ceil (maxTime / settings.plInterval));
                        std::vector<double> refSamples ((size_t) n + 1);
                        for (int i = 0; i <= n; ++i)
                            refSamples[(size_t) i] = ref.valueAt (std::min (maxTime, i * settings.plInterval));

                        std::vector<double> sorted (refSamples);
                        std::sort (sorted.begin(), sorted.end());
                        double median = sorted[sorted.size() / 2];

                        if (median > 0.0)
                        {
                            double globalPeak = 0.0;
                            for (auto& p : partials)
                                for (auto it = p.begin(); it != p.end(); ++it)
                                    globalPeak = std::max (globalPeak, it->amplitude());

                            double lowestFreq = 0.0;
                            if (globalPeak > 0.0)
                            {
                                const double ampFloorLimit = 0.01 * globalPeak;
                                double best = std::numeric_limits<double>::max();
                                for (auto& p : partials)
                                {
                                    double peakAmp = 0.0, peakFreq = 0.0;
                                    for (auto it = p.begin(); it != p.end(); ++it)
                                        if (it->amplitude() > peakAmp)
                                        {
                                            peakAmp = it->amplitude();
                                            peakFreq = it->frequency();
                                        }
                                    if (peakAmp > ampFloorLimit)
                                        best = std::min (best, peakFreq);
                                }
                                if (best < std::numeric_limits<double>::max())
                                    lowestFreq = best;
                            }

                            if (lowestFreq > 0.0)
                            {
                                const double ratio = median / lowestFreq;
                                if (ratio >= 1.6 && ratio <= 2.5)
                                {
                                    for (auto& v : refSamples)
                                        v *= 0.5;
                                    median *= 0.5;
                                }
                            }
                        }

                        const double lo = 0.5 * median;
                        const double hi = 2.0 * median;

                        Loris::LinearEnvelope repaired;
                        for (int i = 0; i <= n; ++i)
                        {
                            const double t = std::min (maxTime, i * settings.plInterval);
                            double v = refSamples[(size_t) i];
                            if (v < lo || v > hi)
                            {
                                int best = -1;
                                for (int d = 1; d <= n && best < 0; ++d)
                                {
                                    for (int dir = -1; dir <= 1; dir += 2)
                                    {
                                        const int j = i + dir * d;
                                        if (j >= 0 && j <= n && refSamples[(size_t) j] >= lo && refSamples[(size_t) j] <= hi)
                                        {
                                            best = j;
                                            break;
                                        }
                                    }
                                }
                                v = best >= 0 ? refSamples[(size_t) best] : median;
                            }
                            repaired.insert (t, v);
                        }
                        ref = repaired;
                    }

                    Loris::Channelizer channelizer (ref, 1);
                    channelizer.channelize (partials.begin(), partials.end());
                    Loris::Distiller distiller;
                    distiller.distill (partials);

                    stats.pipelined = true;
                    stats.pipelineInputPartialCount = before;
                    try
                    {
                        stats.pipelineRefStartHz = ref.valueAt (0.0);
                        stats.pipelineRefMidHz = ref.valueAt (maxTime / 2.0);
                        stats.pipelineRefEndHz = ref.valueAt (maxTime);
                    }
                    catch (...)
                    {
                    }

                    double newMaxTime = 0.0;
                    for (auto& p : partials)
                        newMaxTime = std::max (newMaxTime, p.endTime());
                    maxTime = newMaxTime;
                }

                if (settings.topEndBoostDb > 0.0 && settings.topEndBoostHz > 0.0)
                {
                    const double gain = std::pow (10.0, settings.topEndBoostDb / 20.0);
                    size_t boostedCount = 0;
                    for (auto& p : partials)
                    {
                        if (p.numBreakpoints() == 0)
                            continue;

                        double freqSum = 0.0, bwSum = 0.0, ampSum = 0.0;
                        int nBp = 0;
                        for (auto it = p.begin(); it != p.end(); ++it)
                        {
                            const double a = it->amplitude();
                            freqSum += it->frequency();
                            bwSum += a * it->bandwidth();
                            ampSum += a;
                            ++nBp;
                        }
                        const double midFreq = freqSum / nBp;
                        const double avgBw = ampSum > 0.0 ? bwSum / ampSum : 0.0;
                        if (midFreq <= settings.topEndBoostHz || avgBw <= 0.5)
                            continue;

                        for (auto it = p.begin(); it != p.end(); ++it)
                            it.breakpoint().setAmplitude (it.breakpoint().amplitude() * gain);
                        ++boostedCount;
                    }
                    stats.boostedPartialCount = boostedCount;
                }

                if (settings.hiCutHz > 0.0)
                {
                    Loris::PartialList kept;
                    for (auto& p : partials)
                    {
                        double peakFreq = 0.0;
                        for (auto it = p.begin(); it != p.end(); ++it)
                            peakFreq = std::max (peakFreq, it->frequency());
                        if (peakFreq <= settings.hiCutHz)
                            kept.push_back (p);
                    }
                    partials = std::move (kept);

                    maxTime = 0.0;
                    for (auto& p : partials)
                        maxTime = std::max (maxTime, p.endTime());
                }

                if (settings.keepPartialData)
                {
                    PartialFrameData& pf = result.partials;
                    pf.onsets = onsetTimes;

                    std::vector<std::pair<double, int>> activity;
                    activity.reserve (partials.size() * 2);

                    for (auto& p : partials)
                    {
                        const int startIdx = (int) pf.times.size();
                        for (auto it = p.begin(); it != p.end(); ++it)
                        {
                            pf.times.push_back ((float) it.time());
                            pf.freqs.push_back ((float) it->frequency());
                            pf.amps.push_back ((float) it->amplitude());
                            pf.bws.push_back ((float) it->bandwidth());
                        }
                        pf.offsets.push_back (startIdx);

                        if (p.numBreakpoints() > 0)
                        {
                            activity.emplace_back (p.startTime(), +1);
                            activity.emplace_back (p.endTime(), -1);
                            for (auto it = p.begin(); it != p.end(); ++it)
                            {
                                const double f = it->frequency();
                                pf.minFrequencyHz = (pf.minFrequencyHz > 0.0) ? std::min (pf.minFrequencyHz, f) : f;
                                pf.maxFrequencyHz = std::max (pf.maxFrequencyHz, f);
                                pf.durationSeconds = std::max (pf.durationSeconds, it.time());
                            }
                        }
                    }
                    pf.offsets.push_back ((int) pf.times.size());

                    if (! activity.empty())
                    {
                        std::sort (activity.begin(), activity.end(),
                                   [] (const auto& a, const auto& b) { return a.first < b.first; });
                        int active = 0;
                        size_t best = 0;
                        double bestT = 0.0;
                        for (const auto& ev : activity)
                        {
                            active += ev.second;
                            if (active > 0 && (size_t) active > best)
                            {
                                best = (size_t) active;
                                bestT = ev.first;
                            }
                        }
                        pf.maxActiveCount = best;
                        pf.maxActiveTime = bestT;
                    }
                }

                {
                    double finalMinFreq = 1.0e9, finalMaxFreq = 0.0;
                    size_t finalBreakpoints = 0;
                    for (auto& p : partials)
                    {
                        finalBreakpoints += (size_t) std::distance (p.begin(), p.end());
                        for (auto it = p.begin(); it != p.end(); ++it)
                        {
                            finalMinFreq = std::min (finalMinFreq, it->frequency());
                            finalMaxFreq = std::max (finalMaxFreq, it->frequency());
                        }
                    }
                    stats.partialCount = partials.size();
                    stats.breakpointCount = finalBreakpoints;
                    stats.durationSeconds = maxTime;
                    if (! partials.empty())
                    {
                        stats.minFrequencyHz = finalMinFreq;
                        stats.maxFrequencyHz = finalMaxFreq;
                    }
                }

                if (! report (Stage::Writing, 0.85f))
                    return cancelled();

                if (settings.writeFiles)
                {
                    Loris::SdifFile sdif (partials.begin(), partials.end());
                    for (size_t i = 0; i < onsetTimes.size(); ++i)
                        sdif.markers().emplace_back (onsetTimes[i], "onset" + std::to_string (i + 1));
                    result.sdifPath = outputBaseNoExt + ".sdif";
                    sdif.write (result.sdifPath);
                }

                if (settings.writeFiles && settings.writeAttackSidecar)
                {
                    double peak = 0.0;
                    for (double s : samples)
                        peak = std::max (peak, std::abs (s));
                    std::vector<double> flat (samples);
                    if (peak > 0.99)
                        for (double& v : flat)
                            v *= 0.99 / peak;

                    result.sidecarPath = outputBaseNoExt + "_attack.aif";
                    try
                    {
                        Loris::AiffFile af (flat, sampleRate);
                        af.write (result.sidecarPath, 24);
                    }
                    catch (const std::exception&)
                    {

                        result.sidecarPath.clear();
                    }
                }

                if (settings.renderResynthesis)
                {
                    std::vector<double> buffer;
                    Loris::Synthesizer synth (sampleRate, buffer);
                    synth.synthesize (partials.begin(), partials.end());

                    const int fadeN = std::max (1, (int) std::lround (0.001 * sampleRate));
                    const int fadeLen = (int) std::min ((size_t) fadeN, buffer.size() / 2);
                    for (int i = 0; i < fadeLen; ++i)
                    {
                        const double g = (double) i / (double) fadeLen;
                        buffer[(size_t) i] *= g;
                        buffer[buffer.size() - 1 - (size_t) i] *= g;
                    }
                    double peak = 0.0;
                    for (double s : buffer)
                        peak = std::max (peak, std::abs (s));
                    if (peak > 0.0)
                        for (double& s : buffer)
                            s *= 1.0 / peak;

                    result.renderedSamples = std::move (buffer);
                    result.renderedSampleRate = sampleRate;
                    if (settings.writeResynthFile)
                        writeWavMono16 (outputBaseNoExt + "_resyn.wav", sampleRate, result.renderedSamples);
                }

                if (! report (Stage::Done, 1.0f))
                    return cancelled();
            }
            catch (const std::exception& e)
            {
                return fail (std::string ("analysis failed: ") + e.what());
            }
            catch (...)
            {
                return fail ("analysis failed: unknown exception");
            }

            if (outResult)
                *outResult = std::move (result);
            return true;
        }

        bool synthesizePartials (const PartialFrameData& frames,
                                 double sampleRate,
                                 std::vector<double>& outSamples,
                                 std::string* err)
        {
            try
            {
                if (frames.offsets.size() < 2 || frames.times.empty())
                {
                    if (err) *err = "no partials to synthesize";
                    return false;
                }

                Loris::PartialList partials;
                const int nPartials = (int) frames.offsets.size() - 1;
                for (int p = 0; p < nPartials; ++p)
                {
                    Loris::Partial lp;
                    for (int i = frames.offsets[(size_t) p]; i < frames.offsets[(size_t) p + 1]; ++i)
                        lp.insert (frames.times[(size_t) i],
                                   Loris::Breakpoint (frames.freqs[(size_t) i],
                                                      frames.amps[(size_t) i],
                                                      frames.bws[(size_t) i]));
                    if (lp.numBreakpoints() > 0)
                        partials.push_back (lp);
                }
                if (partials.empty())
                {
                    if (err) *err = "no usable partials after rebuild";
                    return false;
                }

                outSamples.clear();
                Loris::Synthesizer synth (sampleRate, outSamples);
                synth.synthesize (partials.begin(), partials.end());
                if (outSamples.empty())
                {
                    if (err) *err = "synthesis produced no audio";
                    return false;
                }

                const int fadeN = std::max (1, (int) std::lround (0.001 * sampleRate));
                const int fadeLen = (int) std::min ((size_t) fadeN, outSamples.size() / 2);
                for (int i = 0; i < fadeLen; ++i)
                {
                    const double g = (double) i / (double) fadeLen;
                    outSamples[(size_t) i] *= g;
                    outSamples[outSamples.size() - 1 - (size_t) i] *= g;
                }
                double peak = 0.0;
                for (double s : outSamples)
                    peak = std::max (peak, std::abs (s));
                if (peak > 0.0)
                    for (double& s : outSamples)
                        s *= 1.0 / peak;

                return true;
            }
            catch (const std::exception& e)
            {
                if (err) *err = std::string ("synthesis failed: ") + e.what();
                return false;
            }
            catch (...)
            {
                if (err) *err = "synthesis failed: unknown exception";
                return false;
            }
        }
    }
}
