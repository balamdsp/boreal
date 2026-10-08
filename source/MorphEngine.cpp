#include "MorphEngine.h"

#include "AiffFile.h"

#include <algorithm>
#include <cmath>
#include <limits>
MorphEngine::MorphEngine() = default;
MorphEngine::~MorphEngine() = default;

bool MorphEngine::loadSlot (int slotIndex, const juce::File& sdifFile)
{
    juce::String ignored;
    return loadSlot (slotIndex, sdifFile, ignored);
}

bool MorphEngine::loadSlot (int slotIndex, const juce::File& sdifFile, juce::String& errorMessage)
{
    jassert (slotIndex >= 0 && slotIndex < numSlots);
    if (! sdifFile.existsAsFile())
    {
        errorMessage = "File not found: " + sdifFile.getFullPathName();
        return false;
    }

    std::lock_guard<std::mutex> lock (morphMutex);

    auto& slot = slots[(size_t) slotIndex];
    slot.loaded = false;
    slot.byLabel.clear();
    slot.unlabeled.clear();
    slot.duration = 0.0;
    slot.rawDurationSeconds = 0.0;
    slot.attackSamples.clear();
    slot.attackSampleRate = 0.0;
    slot.onsetTimes.clear();
    slot.hasAttackLayer = false;
    slot.filePath = sdifFile.getFullPathName();
    loadedMask.fetch_and (~ (1 << slotIndex));

    try
    {

        Loris::SdifFile importedFile (sdifFile.getFullPathName().toStdString());

        slot.partials = std::make_unique<Loris::PartialList> (importedFile.partials());

        for (auto& partial : *slot.partials)
            slot.rawDurationSeconds = juce::jmax (slot.rawDurationSeconds, partial.endTime());

        channelizeAndDistill (slot);
        buildLabelIndexAndDuration (slot);
        fitSlotFormantEnvelope (slot);

        try
        {
            std::vector<double> onsets;
            for (const auto& m : importedFile.markers())
                if (m.name().rfind ("onset", 0) == 0)
                    onsets.push_back (m.time());
            if (! onsets.empty())
            {
                std::sort (onsets.begin(), onsets.end());

                const juce::String base = sdifFile.getFileNameWithoutExtension();
                const juce::File sidecar = sdifFile.getParentDirectory()
                                               .getChildFile (base + "_attack.aif");
                if (sidecar.existsAsFile())
                {
                    Loris::AiffFile af (sidecar.getFullPathName().toStdString());
                    if (af.sampleRate() > 0.0 && ! af.samples().empty())
                    {
                        slot.attackSamples.reserve (af.samples().size());
                        for (double s : af.samples())
                            slot.attackSamples.push_back ((float) s);
                        slot.attackSampleRate = af.sampleRate();
                        slot.onsetTimes = std::move (onsets);
                        slot.hasAttackLayer = true;
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            (void) e;
            DBG ("MorphEngine: attack layer unavailable for "
                 << sdifFile.getFileName() << ": " << e.what());
            slot.attackSamples.clear();
            slot.attackSampleRate = 0.0;
            slot.onsetTimes.clear();
            slot.hasAttackLayer = false;
        }

        slot.loaded = true;
        errorMessage.clear();
        loadedMask.fetch_or (1 << slotIndex);
        return true;
    }
    catch (const std::exception& e)
    {
        DBG ("MorphEngine::loadSlot failed: " << e.what());
        errorMessage = "Couldn't import " + sdifFile.getFileName() + ": " + e.what();
        slot.loaded = false;
        slot.filePath = {};
        return false;
    }
}

void MorphEngine::clearAllSlots()
{
    std::lock_guard<std::mutex> lock (morphMutex);
    for (auto& slot : slots)
    {
        slot.partials.reset();
        slot.byLabel.clear();
        slot.unlabeled.clear();
        slot.duration = 0.0;
        slot.rawDurationSeconds = 0.0;
        slot.referenceFreq = 0.0;
        slot.attackSamples.clear();
        slot.attackSampleRate = 0.0;
        slot.onsetTimes.clear();
        slot.hasAttackLayer = false;
        slot.formantEnvReady = false;
        slot.filePath = {};
        slot.loaded = false;
    }
    loadedMask.store (0);
}

void MorphEngine::clearSlot (int slotIndex)
{
    jassert (slotIndex >= 0 && slotIndex < numSlots);
    std::lock_guard<std::mutex> lock (morphMutex);

    auto& slot = slots[(size_t) slotIndex];
    slot.partials.reset();
    slot.byLabel.clear();
    slot.unlabeled.clear();
    slot.duration = 0.0;
    slot.rawDurationSeconds = 0.0;
    slot.referenceFreq = 0.0;
    slot.attackSamples.clear();
    slot.attackSampleRate = 0.0;
    slot.onsetTimes.clear();
    slot.hasAttackLayer = false;
    slot.formantEnvReady = false;
    slot.filePath = {};
    slot.loaded = false;
    loadedMask.fetch_and (~ (1 << slotIndex));
}

double MorphEngine::estimatedReferenceFrequency (int slotIndex) const noexcept
{
    jassert (slotIndex >= 0 && slotIndex < numSlots);
    std::lock_guard<std::mutex> lock (morphMutex);

    const auto& slot = slots[(size_t) slotIndex];
    return slot.loaded ? slot.referenceFreq : 0.0;
}

double MorphEngine::estimatedReferenceFrequency (float x, float y) const noexcept
{
    std::lock_guard<std::mutex> lock (morphMutex);

    x = juce::jlimit (0.0f, 1.0f, x);
    y = juce::jlimit (0.0f, 1.0f, y);

    double weightedRef = 0.0;
    double totalWeight = 0.0;

    for (int s = 0; s < numSlots; ++s)
    {
        const auto& slot = slots[(size_t) s];
        if (! slot.loaded || slot.referenceFreq <= 0.0)
            continue;

        const float w = weightForSlot (s, x, y);
        weightedRef += (double) w * slot.referenceFreq;
        totalWeight += (double) w;
    }

    if (totalWeight <= 0.0)
        return 0.0;

    return weightedRef / totalWeight;
}

bool MorphEngine::isSlotLoaded (int slotIndex) const noexcept
{
    jassert (slotIndex >= 0 && slotIndex < numSlots);
    std::lock_guard<std::mutex> lock (morphMutex);
    return slots[(size_t) slotIndex].loaded;
}

juce::String MorphEngine::getSlotFilePath (int slotIndex) const
{
    jassert (slotIndex >= 0 && slotIndex < numSlots);
    std::lock_guard<std::mutex> lock (morphMutex);
    return slots[(size_t) slotIndex].filePath;
}

MorphEngine::SlotInfo MorphEngine::getSlotInfo (int slotIndex) const
{
    jassert (slotIndex >= 0 && slotIndex < numSlots);
    std::lock_guard<std::mutex> lock (morphMutex);

    const auto& slot = slots[(size_t) slotIndex];

    SlotInfo info;
    info.loaded = slot.loaded;
    info.filePath = slot.filePath;
    if (slot.loaded)
    {
        const juce::File f (slot.filePath);
        info.fileName = f.getFileNameWithoutExtension();
        info.extension = f.getFileExtension().substring (1).toLowerCase();
        info.durationSeconds = slot.rawDurationSeconds;
        info.labeledChannelCount = slot.byLabel.empty() ? 0 : (int) slot.byLabel.size() - 1;
        info.unlabeledCount = (int) slot.unlabeled.size();
        info.loudnessGain = slot.loudnessGain;
        info.onsetCount = (int) slot.onsetTimes.size();
        info.hasAttackLayer = slot.hasAttackLayer;
    }
    return info;
}

MorphEngine::AttackLayerInfo MorphEngine::getAttackLayer (int slotIndex) const
{
    jassert (slotIndex >= 0 && slotIndex < numSlots);
    std::lock_guard<std::mutex> lock (morphMutex);

    const auto& slot = slots[(size_t) slotIndex];
    AttackLayerInfo info;
    info.present = slot.loaded && slot.hasAttackLayer;
    info.sampleRate = slot.attackSampleRate;
    if (info.present)
    {
        info.samples = slot.attackSamples;
        info.onsetTimes = slot.onsetTimes;
        info.firstOnsetSeconds = info.onsetTimes.empty() ? 0.0 : info.onsetTimes.front();
    }
    return info;
}

void MorphEngine::setLabeledEnabled (bool enabled) noexcept
{
    std::lock_guard<std::mutex> lock (morphMutex);
    labeledEnabled = enabled;
}

void MorphEngine::setUnlabeledEnabled (bool enabled) noexcept
{
    std::lock_guard<std::mutex> lock (morphMutex);
    unlabeledEnabled = enabled;
}

bool MorphEngine::isLabeledEnabled() const noexcept
{
    std::lock_guard<std::mutex> lock (morphMutex);
    return labeledEnabled;
}

bool MorphEngine::isUnlabeledEnabled() const noexcept
{
    std::lock_guard<std::mutex> lock (morphMutex);
    return unlabeledEnabled;
}

void MorphEngine::dilateToCanonicalTimeline (Loris::PartialList& partials)
{
    double rawDuration = 0.0;
    for (auto& partial : partials)
        rawDuration = juce::jmax (rawDuration, partial.endTime());

    if (rawDuration <= 0.0)
        return;

    const std::vector<double> initialTimes {
        0.0,
        rawDuration * attackLandmarkFraction,
        rawDuration
    };
    const std::vector<double> targetTimes {
        0.0,
        canonicalDuration * attackLandmarkFraction,
        canonicalDuration
    };

    Loris::Dilator dilator (initialTimes.begin(), initialTimes.end(), targetTimes.begin());
    dilator.dilate (partials.begin(), partials.end());
}

void MorphEngine::channelizeAndDistill (Slot& slot)
{
    auto& partials = *slot.partials;

    Loris::LinearEnvelope refEnvelope (referenceHz);
    double representativeRefHz = referenceHz;

    try
    {
        double maxTime = 0.0;
        for (auto& partial : partials)
            maxTime = juce::jmax (maxTime, partial.endTime());

        if (maxTime > 0.0)
        {

            Loris::FundamentalFromPartials estimator;
            refEnvelope = estimator.buildEnvelope (partials, 0.0, maxTime,
                                                    0.01,
                                                    50.0,
                                                    5000.0,
                                                    0.9);
        }
    }
    catch (const std::exception& e)
    {
        (void) e;
        DBG ("Fundamental estimation failed, using constant reference fallback: " << e.what());
        refEnvelope = Loris::LinearEnvelope (referenceHz);
    }

    {
        double envMaxTime = 0.0;
        for (auto& partial : partials)
            envMaxTime = juce::jmax (envMaxTime, partial.endTime());

        if (envMaxTime > 0.0)
        {
            const double interval = 0.01;
            const int n = std::max (1, (int) std::ceil (envMaxTime / interval));

            std::vector<double> samples ((size_t) n + 1);
            for (int i = 0; i <= n; ++i)
                samples[(size_t) i] = refEnvelope.valueAt (std::min (envMaxTime, i * interval));

            std::vector<double> sorted (samples);
            std::sort (sorted.begin(), sorted.end());
            double median = sorted[sorted.size() / 2];

            if (median > 0.0)
            {
                double globalPeak = 0.0;
                for (auto& partial : partials)
                    for (auto it = partial.begin(); it != partial.end(); ++it)
                        globalPeak = juce::jmax (globalPeak, it->amplitude());

                double lowestFreq = 0.0;
                if (globalPeak > 0.0)
                {
                    const double floor = 0.01 * globalPeak;
                    double best = std::numeric_limits<double>::max();
                    for (auto& partial : partials)
                    {
                        double peakAmp = 0.0, peakFreq = 0.0;
                        for (auto it = partial.begin(); it != partial.end(); ++it)
                            if (it->amplitude() > peakAmp)
                            {
                                peakAmp = it->amplitude();
                                peakFreq = it->frequency();
                            }
                        if (peakAmp > floor)
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
                        for (auto& v : samples)
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
                const double t = std::min (envMaxTime, i * interval);
                double v = samples[(size_t) i];
                if (v < lo || v > hi)
                {
                    int best = -1;
                    for (int d = 1; d <= n && best < 0; ++d)
                    {
                        for (int dir = -1; dir <= 1; dir += 2)
                        {
                            const int j = i + dir * d;
                            if (j >= 0 && j <= n && samples[(size_t) j] >= lo && samples[(size_t) j] <= hi)
                            {
                                best = j;
                                break;
                            }
                        }
                    }
                    v = best >= 0 ? samples[(size_t) best] : median;
                }
                repaired.insert (t, v);
            }

            refEnvelope = repaired;
            if (median > 0.0)
                representativeRefHz = median;
        }
    }

    Loris::Channelizer channelizer (refEnvelope, 1);

    channelizer.channelize (partials.begin(), partials.end());

    Loris::Distiller distiller;
    distiller.distill (partials);

    bridgeShortGaps (partials);

    slot.referenceFreq = representativeRefHz;
}

void MorphEngine::bridgeShortGaps (Loris::PartialList& partials)
{

    constexpr double silenceThreshold = 1.0e-6;

    for (auto& p : partials)
    {
        if (p.label() <= 0 || p.numBreakpoints() < 2)
            continue;

        std::vector<std::pair<double, double>> gapSpans;

        Loris::Partial::iterator it = p.begin();
        while (it != p.end())
        {
            if (it.breakpoint().amplitude() > silenceThreshold)
            {
                ++it;
                continue;
            }

            if (it == p.begin())
            {

                while (it != p.end() && it.breakpoint().amplitude() <= silenceThreshold)
                    ++it;
                continue;
            }

            Loris::Partial::iterator before = it;
            --before;

            Loris::Partial::iterator runEnd = it;
            while (runEnd != p.end() && runEnd.breakpoint().amplitude() <= silenceThreshold)
                ++runEnd;

            if (runEnd == p.end())
                break;

            const double gapStart = before.time();
            const double gapEnd   = runEnd.time();
            if (gapEnd - gapStart <= maxGapSeconds)
                gapSpans.emplace_back (gapStart, gapEnd);

            it = runEnd;
        }

        for (auto& span : gapSpans)
        {
            Loris::Partial::iterator b = p.findAfter (span.first);
            if (b != p.end() && b.time() <= span.first)
                ++b;
            Loris::Partial::iterator e = p.findAfter (span.second);

            if (b != e)
                p.erase (b, e);
        }
    }
}

void MorphEngine::buildLabelIndexAndDuration (Slot& slot)
{
    int maxLabel = 0;
    double maxEndTime = 0.0;

    for (auto& partial : *slot.partials)
    {
        maxLabel = juce::jmax (maxLabel, partial.label());
        maxEndTime = juce::jmax (maxEndTime, partial.endTime());
    }

    slot.byLabel.assign ((size_t) maxLabel + 1, nullptr);
    slot.duration = maxEndTime;

    std::vector<const Loris::Partial*> unlabeledCandidates;

    for (auto& partial : *slot.partials)
    {
        const int label = partial.label();
        if (label > 0 && label <= maxLabel)
            slot.byLabel[(size_t) label] = &partial;
        else if (label <= 0)
            unlabeledCandidates.push_back (&partial);
    }

    if ((int) unlabeledCandidates.size() > maxUnlabeledPerSlot)
    {

        std::sort (unlabeledCandidates.begin(), unlabeledCandidates.end(),
                   [] (const Loris::Partial* a, const Loris::Partial* b)
                   {
                       const double ta = a->endTime() * 0.5;
                       const double tb = b->endTime() * 0.5;
                       return a->amplitudeAt (ta) > b->amplitudeAt (tb);
                   });
        unlabeledCandidates.resize ((size_t) maxUnlabeledPerSlot);
    }

    slot.unlabeled = std::move (unlabeledCandidates);

    double maxAmpSum = 0.0;
    const int sweepSteps = 256;
    for (int s = 0; s <= sweepSteps; ++s)
    {
        const double t = maxEndTime * (double) s / (double) sweepSteps;
        double sum = 0.0;
        for (int label = 1; label < (int) slot.byLabel.size(); ++label)
            if (slot.byLabel[(size_t) label] != nullptr)
                sum += slot.byLabel[(size_t) label]->amplitudeAt (t);
        for (const Loris::Partial* p : slot.unlabeled)
            sum += p->amplitudeAt (t);
        maxAmpSum = juce::jmax (maxAmpSum, sum);
    }
    slot.loudnessGain = maxAmpSum > 1.0e-6 ? (float) (1.0 / maxAmpSum) : 1.0f;
}

float MorphEngine::weightForSlot (int slotIndex, float x, float y)
{

    switch (slotIndex)
    {
        case 0: return (1.0f - x) * y;
        case 1: return x * y;
        case 2: return (1.0f - x) * (1.0f - y);
        case 3: return x * (1.0f - y);
        default: jassertfalse; return 0.0f;
    }
}

void MorphEngine::fitSlotFormantEnvelope (Slot& slot)
{
    slot.formantEnvReady = false;
    if (slot.partials == nullptr
        || (slot.byLabel.empty() && slot.unlabeled.empty()))
        return;

    std::vector<float> freqs, amps;
    freqs.reserve (8192);
    amps.reserve (8192);
    auto collect = [&] (const Loris::Partial* p)
    {
        if (p == nullptr)
            return;
        for (auto it = p->begin(); it != p->end(); ++it)
        {
            const double f = it->frequency();
            const double a = it->amplitude();
            if (f > 0.0 && a > 0.0)
            {
                freqs.push_back ((float) f);
                amps.push_back ((float) a);
            }
        }
    };
    for (const Loris::Partial* p : slot.byLabel) collect (p);
    for (const Loris::Partial* p : slot.unlabeled) collect (p);

    constexpr size_t kMaxFitPoints = 200000;
    if (freqs.size() > kMaxFitPoints)
    {
        const size_t stride = freqs.size() / kMaxFitPoints;
        size_t w = 0;
        for (size_t r = 0; r < freqs.size(); r += stride)
        {
            freqs[w] = freqs[r];
            amps[w] = amps[r];
            ++w;
        }
        freqs.resize (w);
        amps.resize (w);
    }

    fitFormantEnvelope (freqs.data(), amps.data(), freqs.size(), slot.formantEnv,
                        slot.formantEnvLogLo, slot.formantEnvLogHi);
    slot.formantEnvReady = true;
}

void MorphEngine::fitFormantEnvelope (const float* freqs, const float* amps,
                                         size_t n, float* envOut,
                                         float& logLoOut, float& logHiOut)
{
    constexpr int B = formantEnvBins;
    logLoOut = std::log (formantEnvLoHz);
    logHiOut = std::log (formantEnvHiHz);

    float dataMin = 0.0f, dataMax = 0.0f;
    int valid = 0;
    for (size_t i = 0; i < n; ++i)
    {
        if (! (freqs[i] > 0.0f) || ! (amps[i] > 0.0f))
            continue;
        dataMin = valid == 0 ? freqs[i] : juce::jmin (dataMin, freqs[i]);
        dataMax = valid == 0 ? freqs[i] : juce::jmax (dataMax, freqs[i]);
        ++valid;
    }

    if (valid < 2)
    {
        for (int b = 0; b < B; ++b) envOut[b] = 1.0f;
        return;
    }

    const float lo = juce::jmax (20.0f, dataMin * 0.5f);
    const float hi = juce::jmin (20000.0f, dataMax * 2.0f);
    logLoOut = std::log (lo);
    logHiOut = std::log (juce::jmax (lo * 1.01f, hi));
    const float logSpan = logHiOut - logLoOut;

    float bins[B] = {};
    for (size_t i = 0; i < n; ++i)
    {
        const float f = freqs[i], a = amps[i];
        if (! (f > 0.0f) || ! (a > 0.0f))
            continue;
        const float u = std::log (f) - logLoOut;
        int b = (int) (u / logSpan * (float) B);
        bins[(size_t) juce::jlimit (0, B - 1, b)] += a;
    }

    float peak = 0.0f;
    for (int b = 0; b < B; ++b) peak = juce::jmax (peak, bins[b]);
    const float floor = peak * std::pow (10.0f, formantEnvFloorDb / 20.0f);
    for (int b = 0; b < B; ++b) bins[b] = juce::jmax (bins[b], floor);

    float tmp[B];
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int b = 0; b < B; ++b)
        {
            const float m2 = bins[(size_t) juce::jlimit (0, B - 1, b - 2)];
            const float m1 = bins[(size_t) juce::jlimit (0, B - 1, b - 1)];
            const float p1 = bins[(size_t) juce::jlimit (0, B - 1, b + 1)];
            const float p2 = bins[(size_t) juce::jlimit (0, B - 1, b + 2)];
            tmp[(size_t) b] = (m2 + 4.0f * m1 + 6.0f * bins[(size_t) b] + 4.0f * p1 + p2) / 16.0f;
        }
        for (int b = 0; b < B; ++b) bins[(size_t) b] = tmp[(size_t) b];
    }

    for (int b = 0; b < B; ++b) envOut[b] = bins[(size_t) b];
}

float MorphEngine::formantCorrRatio (const float* envBins, float logLo,
                                     float logHi, float freqHz, float shiftSt)
{
    if (std::fabs (shiftSt) < 1.0e-6f || ! (freqHz > 0.0f))
        return 1.0f;

    constexpr int B = formantEnvBins;
    const float logSpan = juce::jmax (1.0e-6f, logHi - logLo);

    auto envAt = [&] (float f)
    {
        const float clamped = juce::jmax (std::exp (logLo),
                                          juce::jmin (std::exp (logHi), f));
        const float u = (std::log (clamped) - logLo) / logSpan;
        const float pos = juce::jlimit (0.0f, (float) (B - 1), u * (float) B);
        const int b0 = juce::jmin (B - 2, (int) pos);
        const float frac = pos - (float) b0;
        return envBins[(size_t) b0] * (1.0f - frac) + envBins[(size_t) b0 + 1] * frac;
    };

    const float shiftMult = std::pow (2.0f, shiftSt / 12.0f);
    const float denom = envAt (freqHz);
    if (! (denom > 0.0f))
        return 1.0f;
    float ratio = envAt (freqHz * shiftMult) / denom;

    const float maxRatio = std::pow (10.0f, formantMaxRatioDb / 20.0f);
    return juce::jlimit (1.0f / maxRatio, maxRatio, ratio);
}

bool MorphEngine::computeMorph (float x, float y, double normalizedTime,
                                  std::vector<OscillatorTarget>& outTargets,
                                  float xFreq, float xAmp, float xBw,
                                  const double* slotTimes,
                                  const float* formantShiftSt)
{
    std::lock_guard<std::mutex> lock (morphMutex);

    bool anyLoaded = false;
    for (int i = 0; i < numSlots; ++i)
        if (slots[(size_t) i].loaded) { anyLoaded = true; break; }
    if (! anyLoaded)
        return false;

    x = juce::jlimit (0.0f, 1.0f, x);
    y = juce::jlimit (0.0f, 1.0f, y);
    normalizedTime = juce::jlimit (0.0, 1.0, normalizedTime);

    const float xF = xFreq >= 0.0f ? juce::jlimit (0.0f, 1.0f, xFreq) : x;
    const float xA = xAmp  >= 0.0f ? juce::jlimit (0.0f, 1.0f, xAmp)  : x;
    const float xB = xBw   >= 0.0f ? juce::jlimit (0.0f, 1.0f, xBw)   : x;

    float totalAmpWeight = 0.0f;
    for (int s = 0; s < numSlots; ++s)
        if (slots[(size_t) s].loaded)
            totalAmpWeight += weightForSlot (s, xA, y);
    if (totalAmpWeight <= 1.0e-6f)
    {
        outTargets.clear();
        return true;
    }
    const float ampNorm = 1.0f / totalAmpWeight;

    outTargets.clear();

    if (labeledEnabled)
    {

        int maxLabel = 0;
        for (auto& slot : slots)
            maxLabel = juce::jmax (maxLabel, (int) slot.byLabel.size() - 1);

        std::vector<OscillatorTarget> labeledTargets;
        labeledTargets.reserve ((size_t) juce::jmax (0, maxLabel));

        for (int label = 1; label <= maxLabel; ++label)
        {
            float freqWeightedSum = 0.0f;
            float ampWeightedSum  = 0.0f;
            float bwWeightedSum   = 0.0f;
            float freqWeight      = 0.0f;
            float ampWeight       = 0.0f;
            float bwWeight        = 0.0f;

            for (int s = 0; s < numSlots; ++s)
            {
                const auto& slot = slots[(size_t) s];
                if (label >= (int) slot.byLabel.size())
                    continue;

                const Loris::Partial* p = slot.byLabel[(size_t) label];
                if (p == nullptr)
                    continue;

                const double slotNt = slotTimes != nullptr
                    ? juce::jlimit (0.0, 1.0, slotTimes[s]) : normalizedTime;
                const double t = slotNt * slot.duration;

                float freq, amp;
                freq = (float) p->frequencyAt (t);
                amp  = (float) p->amplitudeAt (t) * slot.loudnessGain;
                if (formantShiftSt != nullptr
                    && std::fabs (formantShiftSt[s]) >= 1.0e-6f
                    && slot.formantEnvReady)
                    amp *= formantCorrRatio (slot.formantEnv, slot.formantEnvLogLo,
                                             slot.formantEnvLogHi, freq,
                                             formantShiftSt[s]);
                const float bw   = (float) p->bandwidthAt (t);

                const float wF = weightForSlot (s, xF, y);
                const float wA = weightForSlot (s, xA, y);
                const float wB = weightForSlot (s, xB, y);

                freqWeightedSum += wF * freq;
                ampWeightedSum  += wA * amp;
                bwWeightedSum   += wB * bw;
                freqWeight      += wF;
                ampWeight       += wA;
                bwWeight        += wB;
            }

            if (freqWeight <= 0.0f && ampWeight <= 0.0f && bwWeight <= 0.0f)
                continue;

            OscillatorTarget target;

            target.frequencyHz = freqWeight > 0.0f ? freqWeightedSum / freqWeight : 0.0f;
            target.amplitude   = ampWeightedSum * ampNorm;
            target.bandwidth   = bwWeight > 0.0f ? juce::jlimit (0.0f, 1.0f, bwWeightedSum / bwWeight) : 0.0f;
            target.sourceId    = label;
            labeledTargets.push_back (target);
        }

        if constexpr (maxLabeledPerTick > 0)
        {
            if ((int) labeledTargets.size() > maxLabeledPerTick)
            {
                std::partial_sort (labeledTargets.begin(),
                                   labeledTargets.begin () + maxLabeledPerTick,
                                   labeledTargets.end(),
                                   [] (const OscillatorTarget& a, const OscillatorTarget& b)
                                   { return a.amplitude > b.amplitude; });
                labeledTargets.resize ((size_t) maxLabeledPerTick);
            }
        }

        outTargets.insert (outTargets.end(), labeledTargets.begin(), labeledTargets.end());
    }

    if (unlabeledEnabled)
    {
        for (int s = 0; s < numSlots; ++s)
        {
            const auto& slot = slots[(size_t) s];
            if (! slot.loaded || slot.unlabeled.empty())
                continue;

            const float w = weightForSlot (s, xA, y);
            if (w <= 0.0f)
                continue;

            const double slotNt = slotTimes != nullptr
                ? juce::jlimit (0.0, 1.0, slotTimes[s]) : normalizedTime;
            const double t = slotNt * slot.duration;

            std::vector<OscillatorTarget> cornerCandidates;
            cornerCandidates.reserve (slot.unlabeled.size());

            const float slotShift = (formantShiftSt != nullptr) ? formantShiftSt[s] : 0.0f;
            const bool useFormant = std::fabs (slotShift) >= 1.0e-6f && slot.formantEnvReady;

            for (const Loris::Partial* p : slot.unlabeled)
            {
                const float freqHz = (float) p->frequencyAt (t);
                float amp = w * (float) p->amplitudeAt (t) * slot.loudnessGain * ampNorm;
                if (useFormant)
                    amp *= formantCorrRatio (slot.formantEnv, slot.formantEnvLogLo,
                                             slot.formantEnvLogHi, freqHz, slotShift);
                if (amp <= 0.0001f)
                    continue;

                OscillatorTarget candidate;
                candidate.frequencyHz = freqHz;
                candidate.amplitude   = amp;
                candidate.bandwidth   = juce::jlimit (0.0f, 1.0f, (float) p->bandwidthAt (t));

                candidate.sourceId    = (int)(intptr_t) p | 0x40000000;
                cornerCandidates.push_back (candidate);
            }

            if ((int) cornerCandidates.size() > maxUnlabeledPerCorner)
            {

                std::partial_sort (cornerCandidates.begin(),
                                   cornerCandidates.begin() + maxUnlabeledPerCorner,
                                   cornerCandidates.end(),
                                   [] (const OscillatorTarget& a, const OscillatorTarget& b)
                                   { return a.amplitude > b.amplitude; });
                cornerCandidates.resize ((size_t) maxUnlabeledPerCorner);
            }

            outTargets.insert (outTargets.end(), cornerCandidates.begin(), cornerCandidates.end());
        }
    }

    return true;
}

int MorphEngine::getMaxChannels() const noexcept
{
    std::lock_guard<std::mutex> lock (morphMutex);
    int maxLabel = 0;
    for (auto& slot : slots)
        maxLabel = juce::jmax (maxLabel, (int) slot.byLabel.size() - 1);
    return maxLabel;
}

double MorphEngine::estimatedRealtimeDuration (float x, float y) const
{
    std::lock_guard<std::mutex> lock (morphMutex);

    x = juce::jlimit (0.0f, 1.0f, x);
    y = juce::jlimit (0.0f, 1.0f, y);

    double weightedDuration = 0.0;
    double totalWeight = 0.0;

    for (int s = 0; s < numSlots; ++s)
    {
        if (! slots[(size_t) s].loaded)
            continue;

        const float w = weightForSlot (s, x, y);
        weightedDuration += (double) w * slots[(size_t) s].rawDurationSeconds;
        totalWeight += (double) w;
    }

    if (totalWeight <= 0.0)
        return 1.0;

    return weightedDuration / totalWeight;
}

std::vector<MorphEngine::UnlabeledRow> MorphEngine::getUnlabeledRows (int slotIndex) const
{
    std::lock_guard<std::mutex> lock (morphMutex);

    std::vector<UnlabeledRow> rows;
    if (slotIndex < 0 || slotIndex >= numSlots)
        return rows;

    const auto& slot = slots[(size_t) slotIndex];
    if (! slot.loaded)
        return rows;

    rows.reserve (slot.unlabeled.size());
    for (const Loris::Partial* p : slot.unlabeled)
    {
        UnlabeledRow row;
        row.midTime = p->endTime() * 0.5;
        row.frequencyHz = p->frequencyAt (row.midTime);
        row.bandwidth = p->bandwidthAt (row.midTime);
        row.amplitude = p->amplitudeAt (row.midTime);
        row.label = p->label();
        rows.push_back (row);
    }
    return rows;
}

std::vector<MorphEngine::ChannelRow> MorphEngine::getChannelRows (int slotIndex, double normalizedTime) const
{
    std::lock_guard<std::mutex> lock (morphMutex);

    std::vector<ChannelRow> rows;
    if (slotIndex < 0 || slotIndex >= numSlots)
        return rows;

    const auto& slot = slots[(size_t) slotIndex];
    if (! slot.loaded)
        return rows;

    normalizedTime = juce::jlimit (0.0, 1.0, normalizedTime);
    const double t = normalizedTime * slot.duration;

    rows.reserve (slot.byLabel.size());
    for (int label = 1; label < (int) slot.byLabel.size(); ++label)
    {
        const Loris::Partial* p = slot.byLabel[(size_t) label];
        if (p == nullptr)
            continue;
        ChannelRow row;
        row.channel = label;
        row.frequencyHz = p->frequencyAt (t);
        row.bandwidth = p->bandwidthAt (t);
        row.amplitude = p->amplitudeAt (t);
        rows.push_back (row);
    }
    return rows;
}
