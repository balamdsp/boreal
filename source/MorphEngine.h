#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "PartialList.h"
#include "SdifFile.h"
#include "Channelizer.h"
#include "Distiller.h"
#include "Dilator.h"
#include "LinearEnvelope.h"
#include "Fundamental.h"

struct OscillatorTarget
{
    float frequencyHz = 0.0f;
    float amplitude   = 0.0f;
    float bandwidth   = 0.0f;
    int   sourceId    = 0;
};

class MorphEngine
{
public:
    static constexpr int numSlots = 4;

    static constexpr double canonicalDuration = 1.0;
    static constexpr double attackLandmarkFraction = 0.10;

    static constexpr int maxUnlabeledPerSlot = 200;

    static constexpr int maxUnlabeledPerCorner = 200;

    static constexpr int maxLabeledPerTick = 0;

    static constexpr double maxGapSeconds = 0.1;

    MorphEngine();
    ~MorphEngine();

    struct SlotInfo
    {
        bool loaded = false;
        juce::String filePath;
        juce::String fileName;
        juce::String extension;
        double durationSeconds = 0.0;
        int labeledChannelCount = 0;
        int unlabeledCount = 0;
        float loudnessGain = 1.0f;

        int onsetCount = 0;
        bool hasAttackLayer = false;
    };

    bool loadSlot (int slotIndex, const juce::File& sdifFile);

    bool loadSlot (int slotIndex, const juce::File& sdifFile, juce::String& errorMessage);

    int getLoadedMask() const noexcept { return loadedMask.load(); }

    void clearAllSlots();

    void clearSlot (int slotIndex);

    bool isSlotLoaded (int slotIndex) const noexcept;

    juce::String getSlotFilePath (int slotIndex) const;
    SlotInfo getSlotInfo (int slotIndex) const;

    void setLabeledEnabled (bool enabled) noexcept;
    void setUnlabeledEnabled (bool enabled) noexcept;
    bool isLabeledEnabled() const noexcept;
    bool isUnlabeledEnabled() const noexcept;

    void setReferenceFrequency (double hz) noexcept { referenceHz = hz; }

    double estimatedReferenceFrequency (int slotIndex) const noexcept;

        double estimatedReferenceFrequency (float x, float y) const noexcept;

    bool computeMorph (float x, float y, double normalizedTime,
                        std::vector<OscillatorTarget>& outTargets,
                        float xFreq = -1.0f, float xAmp = -1.0f,
                        float xBw = -1.0f, const double* slotTimes = nullptr,
                        const float* formantShiftSt = nullptr);

    int getMaxChannels() const noexcept;

    static constexpr int formantEnvBins = 64;
    static constexpr float formantEnvLoHz = 30.0f;
    static constexpr float formantEnvHiHz = 12000.0f;
    static constexpr float formantEnvFloorDb = -60.0f;
    static constexpr float formantMaxRatioDb = 24.0f;

    static void fitFormantEnvelope (const float* freqs, const float* amps,
                                    size_t n, float* envOut,
                                    float& logLoOut, float& logHiOut);
    static float formantCorrRatio (const float* envBins, float logLo,
                                   float logHi, float freqHz, float shiftSt);

    double estimatedRealtimeDuration (float x, float y) const;

    struct UnlabeledRow
    {
        double frequencyHz = 0.0;
        double bandwidth = 0.0;
        double amplitude = 0.0;
        double midTime = 0.0;
        int label = 0;
    };
    std::vector<UnlabeledRow> getUnlabeledRows (int slotIndex) const;

    struct ChannelRow
    {
        int channel = 0;
        double frequencyHz = 0.0;
        double bandwidth = 0.0;
        double amplitude = 0.0;
    };
    std::vector<ChannelRow> getChannelRows (int slotIndex, double normalizedTime = 0.5) const;

    struct AttackLayerInfo
    {
        bool present = false;
        double firstOnsetSeconds = 0.0;
        double sampleRate = 0.0;
        std::vector<float> samples;
        std::vector<double> onsetTimes;
    };
    AttackLayerInfo getAttackLayer (int slotIndex) const;

private:
    struct Slot
    {
        std::unique_ptr<Loris::PartialList> partials;
        std::vector<const Loris::Partial*> byLabel;
        std::vector<const Loris::Partial*> unlabeled;
        double duration = 0.0;
        double rawDurationSeconds = 0.0;
        double referenceFreq = 0.0;
        float loudnessGain = 1.0f;
        float formantEnv[formantEnvBins] = {};
        float formantEnvLogLo = 0.0f;
        float formantEnvLogHi = 1.0f;
        bool formantEnvReady = false;

        std::vector<float> attackSamples;
        double attackSampleRate = 0.0;
        std::vector<double> onsetTimes;
        bool hasAttackLayer = false;
        juce::String filePath;
        bool loaded = false;
    };

    double referenceHz = 220.0;
    std::array<Slot, numSlots> slots;

    bool labeledEnabled = true;
    bool unlabeledEnabled = true;

    mutable std::mutex morphMutex;

    std::atomic<int> loadedMask { 0 };

    void channelizeAndDistill (Slot& slot);
    void buildLabelIndexAndDuration (Slot& slot);
    static void fitSlotFormantEnvelope (Slot& slot);
    static void dilateToCanonicalTimeline (Loris::PartialList& partials);
    static float weightForSlot (int slotIndex, float x, float y);
    static void bridgeShortGaps (Loris::PartialList& partials);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MorphEngine)
};
