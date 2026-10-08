#pragma once

#include <functional>
#include <string>
#include <vector>

namespace boreal
{
    namespace analyzer
    {

        enum class Stage
        {
            Reading = 0,
            DetectingOnsets,
            TrackingPartials,
            Labeling,

            Writing,
            Done
        };

        const char* stageName (Stage s);

        struct Settings
        {

            double resolution = 60.0;
            double windowWidth = 0.0;
            double ampFloor = 0.0;
            double freqFloor = 0.0;
            double freqDrift = 0.0;
            double hopTime = 0.0;
            double cropTime = 0.0;
            double sidelobe = 0.0;
            std::string bandwidthMode = "residue";
            double bwWidth = 0.0;
            double convergence = 0.0;
            bool phaseCorrect = true;

            double referenceHz = 0.0;
            double fmin = 0.0;
            double fmax = 0.0;
            double sampleRateOverride = 0.0;

            bool cleanPartials = false;
            double plLower = 0.0;
            double plUpper = 0.0;
            double plInterval = 0.01;
            double plConfidence = 0.9;

            double topEndBoostDb = 0.0;
            double topEndBoostHz = 7000.0;

            bool writeAttackSidecar = true;
            double onsetMult = 1.6;

            static constexpr double kOnsetSensMin = 0.25;
            static constexpr double kOnsetSensMax = 1.25;
            static constexpr double kOnsetSensDefault = 0.625;
            static double onsetSensitivityToMult (double sens)
            {
                const double s = sens <= 0.0 ? kOnsetSensDefault : sens;
                const double clamped = s < kOnsetSensMin ? kOnsetSensMin
                    : (s > kOnsetSensMax ? kOnsetSensMax : s);
                return 1.0 / clamped;
            }
            static double onsetMultToSensitivity (double mult)
            {
                const double m = mult <= 0.0 ? 1.6 : mult;
                const double clamped = m < 0.8 ? 0.8 : (m > 4.0 ? 4.0 : m);
                return 1.0 / clamped;
            }

            double maxDurationSeconds = 0.0;

            bool keepPartialData = false;
            bool writeFiles = true;
            double regionStart = 0.0;
            double regionEnd = 1.0;
            double hiCutHz = 0.0;
            bool writeResynthFile = true;

            bool autoMaterial = false;

            static constexpr double kTonalResolutionHz = 200.0;
            static constexpr double kTonalAmpFloorDb = -120.0;
            static constexpr double kPercussiveResolutionHz = 450.0;
            static constexpr double kPercussiveAmpFloorDb = -140.0;

            static constexpr double kAutoOnsetDensityThreshold = 4.0;

            bool renderResynthesis = false;
        };

        struct Stats
        {
            double sourceDurationSeconds = 0.0;
            size_t sourceSampleCount = 0;
            double sampleRate = 0.0;

            double effectiveWidthHz = 0.0;
            double effectiveAmpFloorDb = 0.0;
            double effectiveFreqFloorHz = 0.0;
            double effectiveDriftHz = 0.0;
            double effectiveHopSec = 0.0;
            double effectiveCropSec = 0.0;
            double effectiveSidelobeDb = 0.0;
            std::string trackingDescription;

            size_t onsetCount = 0;

            size_t rawPartialCount = 0;
            size_t rawBreakpointCount = 0;
            double rawMinFrequencyHz = 0.0;
            double rawMaxFrequencyHz = 0.0;
            double rawDurationSeconds = 0.0;

            bool pipelined = false;
            bool pipelineUsedConstantRef = false;
            double pipelineSearchLowHz = 0.0;
            double pipelineSearchHighHz = 0.0;
            size_t pipelineInputPartialCount = 0;
            double pipelineRefStartHz = 0.0;
            double pipelineRefMidHz = 0.0;
            double pipelineRefEndHz = 0.0;

            double autoMaterialDensity = 0.0;
            bool autoResolvedPercussive = false;

            size_t partialCount = 0;
            size_t breakpointCount = 0;
            double durationSeconds = 0.0;
            double minFrequencyHz = 0.0;
            double maxFrequencyHz = 0.0;
            double f0StartHz = 0.0;
            double f0MidHz = 0.0;
            double f0EndHz = 0.0;
            bool hasTrackerF0 = false;

            size_t boostedPartialCount = 0;
        };

        struct PartialFrameData
        {
            std::vector<float> times;
            std::vector<float> freqs;
            std::vector<float> amps;
            std::vector<float> bws;
            std::vector<int> offsets;
            std::vector<double> onsets;

            double durationSeconds = 0.0;
            double minFrequencyHz = 0.0;
            double maxFrequencyHz = 0.0;
            size_t maxActiveCount = 0;
            double maxActiveTime = 0.0;
        };

        struct Result
        {
            std::string sdifPath;
            std::string sidecarPath;
            std::vector<double> onsets;
            std::vector<double> renderedSamples;
            double renderedSampleRate = 0.0;
            Stats stats;
            PartialFrameData partials;
        };

        using ProgressFn = std::function<bool (Stage stage, float overall)>;

        bool readAudioFile (const std::string& path, std::vector<double>& mono,
                            double& sampleRate, std::string& err);

        std::vector<double> detectOnsets (const std::vector<double>& samples,
                                          double sampleRate, double thresholdMult);

        double measureOnsetDensityPerSecond (const std::vector<double>& samples,
                                             double sampleRate);

        bool sdifHasOnsetMarkers (const std::string& sdifPath);

        bool analyzeToFiles (const std::string& inputAudioPath,
                             const std::string& outputBaseNoExt,
                             const Settings& settings,
                             const ProgressFn& progress,
                             Result* outResult,
                             std::string* err);

        bool synthesizePartials (const PartialFrameData& frames,
                                 double sampleRate,
                                 std::vector<double>& outSamples,
                                 std::string* err);

        bool writeWavMono16 (const std::string& path, double sampleRate,
                             const std::vector<double>& samples);
    }
}
