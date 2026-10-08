#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MorphEngine.h"
#include "Oscillator.h"
#include "Breakpoint.h"
#include "Analysis/BorealAnalyzeQueue.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

class BorealPresetManager;

struct MorphSnapshot
{
    std::vector<OscillatorTarget> targets;

    std::array<float, MorphEngine::numSlots> attackBlendShare {};
};

struct SlotAttackLayer
{
    bool present = false;
    bool legacySnippet = false;
    double posOffset = 0.0;
    double sampleRate = 0.0;
    double durationSec = 0.0;
    float loudnessGain = 1.0f;
    std::vector<float> samples;
    std::vector<double> onsets;
};

struct AttackLayerSet
{
    SlotAttackLayer slots[MorphEngine::numSlots];
};

class BorealAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int maxVoices = 8;

    BorealAudioProcessor();
    ~BorealAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    BorealPresetManager* getPresetManager() { return presetManager.get(); }

    void loadSlotAsync (int slotIndex, const juce::File& sdifFile);

    void analyzeAndLoadAsync (int slotIndex, const juce::File& audioFile);

    juce::Value uiModeValue;
    juce::String getUiMode() const { return uiModeValue.getValue().toString(); }
    void setUiMode (const juce::String& m) { uiModeValue = m; }

    void resetAnalyzerDefaults();

    bool hasAnalyzerSource() const { return analyzerSourceRate > 0.0 && ! analyzerSourceSamples.empty(); }
    juce::String getAnalyzerSourceName() const;
    double getAnalyzerSourceDurationSec() const { return hasAnalyzerSource() ? (double) analyzerSourceSamples.size() / analyzerSourceRate : 0.0; }
    double getAnalyzerSampleRate() const { return analyzerSourceRate; }

    int getAnalyzerSourceGeneration() const noexcept { return analyzerSourceGeneration.load(); }

    std::shared_ptr<const std::vector<float>> getAnalyzerSourceBuffer() const
    {
        const juce::SpinLock::ScopedLockType lock (previewLock);
        return previewSourceBuf;
    }

    void analyzerSetInterval (double start, double end) { analyzerIntervalStart = start; analyzerIntervalEnd = end; }
    double getAnalyzerIntervalStart() const { return analyzerIntervalStart; }
    double getAnalyzerIntervalEnd() const { return analyzerIntervalEnd; }

    juce::String openAnalyzerSource (const juce::File& audioFile);
    void clearAnalyzerSession();

    void analyzeScratch();
    void synthesizeScratch();
    void sendScratchToSlot (int slotIndex);

    void postNoteOn (int noteNumber, float velocity)
    {
        std::lock_guard<std::mutex> lock (midiMutex);
        midiQueue.push_back ({ true, noteNumber, juce::jlimit (0.0f, 1.0f, velocity) });
    }
    void postNoteOff (int noteNumber)
    {
        std::lock_guard<std::mutex> lock (midiMutex);
        midiQueue.push_back ({ false, noteNumber, 0.0f });
    }
    float getOutputPeak() const noexcept { return outputPeak.load(); }
    void refreshScratchOnsets();

    std::shared_ptr<const boreal::analyzer::PartialFrameData> getAnalyzerFrames() const
    { return std::atomic_load (&analyzerFrames); }

    const std::vector<double>& getAnalyzerResynth() const { return analyzerResynthSamples; }
    double getAnalyzerResynthRate() const { return analyzerResynthRate; }
    int getAnalyzerJobKind() const noexcept { return analyzerJobKind.load(); }
    float getAnalyzerJobProgress() const noexcept { return analyzeQueue.overallProgress(); }
    juce::String getAnalyzerJobStage() const { return analyzeQueue.currentStageIndex() >= 0 ? slotAnalyzeStageText() : juce::String(); }

    bool exportScratchWav (const juce::File& outFile);

    void exportScratchSdifAsync (const juce::File& destDirectory);

    enum { previewOff = 0, previewSource = 1, previewResynth = 2 };
    void previewStop();
    bool previewPlay (int mode);
    bool previewPlaying() const noexcept { return previewActive.load(); }
    int previewMode() const noexcept { return previewRequestedMode.load(); }
    double previewPlayheadSeconds() const noexcept { return previewPos.load(); }

    juce::Value analyzerWindowWidthHzValue;
    juce::Value analyzerLoCutHzValue;
    juce::Value analyzerHiCutHzValue;
    juce::Value analyzerFreqDriftHzValue;
    juce::Value analyzerNoiseWidthHzValue;
    juce::Value analyzerFundamentalHzValue;
    juce::Value previewSrcVolDbValue;
    juce::Value previewRsynVolDbValue;

    double getAnalyzerWindowWidthHz() const { return (double) analyzerWindowWidthHzValue.getValue(); }
    double getAnalyzerLoCutHz() const { return (double) analyzerLoCutHzValue.getValue(); }
    double getAnalyzerHiCutHz() const { return (double) analyzerHiCutHzValue.getValue(); }
    double getAnalyzerFreqDriftHz() const { return (double) analyzerFreqDriftHzValue.getValue(); }
    double getAnalyzerNoiseWidthHz() const { return (double) analyzerNoiseWidthHzValue.getValue(); }
    double getAnalyzerFundamentalHz() const { return (double) analyzerFundamentalHzValue.getValue(); }

    bool isSlotAnalyzing (int slotIndex) const { return analyzeQueue.busySlot() == slotIndex; }
    float slotAnalyzeProgress() const { return analyzeQueue.overallProgress(); }
    juce::String slotAnalyzeStageText() const;
    int queuedAnalysisCount() const { return analyzeQueue.queuedCount(); }

    juce::String getSlotSourceAudioPath (int slotIndex) const { return slotSourceAudio[(size_t) slotIndex]; }

    void loadSlot (int slotIndex, const juce::File& sdifFile)
    {
        morphEngine.loadSlot (slotIndex, sdifFile);

        refreshAttackLayers();
    }
    void clearAllSlots() { morphEngine.clearAllSlots(); refreshAttackLayers(); for (int i = 0; i < MorphEngine::numSlots; ++i) broadcastSlotRefresh (i); }
    void clearSlot (int slotIndex) { morphEngine.clearSlot (slotIndex); refreshAttackLayers(); broadcastSlotRefresh (slotIndex); }
    juce::String getSlotFilePath (int slotIndex) const { return morphEngine.getSlotFilePath (slotIndex); }
    MorphEngine::SlotInfo getSlotInfo (int slotIndex) const { return morphEngine.getSlotInfo (slotIndex); }

    int getSlotLoadMask() const { return morphEngine.getLoadedMask(); }

    double getTimeIndexDisplay() const noexcept { return timeIndexDisplay.load(); }
    float getSlotPhaseDisplay (int slot) const noexcept
    {
        if (slot < 0 || slot >= MorphEngine::numSlots)
            return -1.0f;
        return slotPhaseDisplay[(size_t) slot].load();
    }

    struct SlotLoadListener
    {
        virtual ~SlotLoadListener() = default;
        virtual void slotLoadFinished (int slotIndex, bool success, const juce::String& errorMessage) = 0;
    };
    void addSlotLoadListener (SlotLoadListener* l) { slotLoadListeners.add (l); }
    void removeSlotLoadListener (SlotLoadListener* l) { slotLoadListeners.remove (l); }
    void broadcastSlotRefresh (int slotIndex)
    {
        slotLoadListeners.call ([&] (SlotLoadListener& l)
        {
            l.slotLoadFinished (slotIndex, true, {});
        });
    }

    void setLabeledEnabled (bool enabled) noexcept { morphEngine.setLabeledEnabled (enabled); }
    void setUnlabeledEnabled (bool enabled) noexcept { morphEngine.setUnlabeledEnabled (enabled); }
    bool isLabeledEnabled() const noexcept { return morphEngine.isLabeledEnabled(); }
    bool isUnlabeledEnabled() const noexcept { return morphEngine.isUnlabeledEnabled(); }

    juce::Value voicesValue;
    juce::Value timeScrubValue;
    juce::Value loopValue;
    juce::Value pitchLockValue;
    juce::Value forwardOnlyValue;
    juce::Value legatoValue;

    juce::Value analyzerResolutionValue;
    juce::Value analyzerAmpFloorValue;
    juce::Value analyzerCleanPartialsValue;
    juce::Value analyzerOnsetSensitivityValue;
    juce::Value analyzerPreRollMsValue;

    juce::Value tooltipDelayMsValue;
    juce::Value tooltipsEnabledValue;
    juce::Value confirmDestructiveValue;

    int getTooltipDelayMs() const { return (int) tooltipDelayMsValue.getValue(); }
    bool getTooltipsEnabled() const { return static_cast<bool> (tooltipsEnabledValue.getValue()); }
    bool getConfirmDestructive() const { return static_cast<bool> (confirmDestructiveValue.getValue()); }

    double getAnalyzerResolutionHz() const { return (double) analyzerResolutionValue.getValue(); }
    double getAnalyzerAmpFloorDb() const { return (double) analyzerAmpFloorValue.getValue(); }
    bool getAnalyzerCleanPartials() const { return static_cast<bool> (analyzerCleanPartialsValue.getValue()); }
    double getAnalyzerOnsetSensitivity() const { return (double) analyzerOnsetSensitivityValue.getValue(); }
    double getAnalyzerOnsetMult() const
    { return boreal::analyzer::Settings::onsetSensitivityToMult (getAnalyzerOnsetSensitivity()); }
    double getAnalyzerPreRollMs() const { return (double) analyzerPreRollMsValue.getValue(); }

    int getCurrentVoices() const { return juce::jlimit (1, maxVoices, (int) voicesValue.getValue()); }
    void setCurrentVoices (int count) { voicesValue = juce::jlimit (1, maxVoices, count); }

    bool getTimeScrubEnabled() const { return static_cast<bool> (timeScrubValue.getValue()); }
    void setTimeScrubEnabled (bool enabled) { timeScrubValue = enabled; }

    bool getLoopEnabled() const { return static_cast<bool> (loopValue.getValue()); }
    void setLoopEnabled (bool enabled) { loopValue = enabled; }

    bool getPitchLockEnabled() const { return static_cast<bool> (pitchLockValue.getValue()); }
    void setPitchLockEnabled (bool enabled) { pitchLockValue = enabled; }

    bool getForwardOnlyEnabled() const { return static_cast<bool> (forwardOnlyValue.getValue()); }
    void setForwardOnlyEnabled (bool enabled) { forwardOnlyValue = enabled; }

    bool getLegatoEnabled() const { return static_cast<bool> (legatoValue.getValue()); }
    void setLegatoEnabled (bool enabled) { legatoValue = enabled; }

    void setCrtEnabled (bool b);
    bool isCrtEnabled() const noexcept { return crtEnabled.load(); }
    const std::atomic<bool>& getCrtEnabledFlag() const noexcept { return crtEnabled; }

    void setCrtStrength (int strength);
    int getCrtStrength() const noexcept { return crtStrength.load(); }
    const std::atomic<int>& getCrtStrengthFlag() const noexcept { return crtStrength; }

    juce::AudioParameterFloat* gainParam = nullptr;

    struct Voice
    {

        double timeIndexPhase = 0.0;
        bool timeIndexRising = true;
        int timeIndexSign = 0;

        struct SlotCursor
        {
            double phase = 0.0;
            bool rising = true;
        };
        std::array<SlotCursor, MorphEngine::numSlots> slotCursors {};

        juce::ADSR envelope;
        bool gateOn = false;

        int noteNumber = -1;
        uint32_t noteOnOrder = 0;

        double glideFactor = -1.0;

        std::shared_ptr<MorphSnapshot> buffers[2];
        std::atomic<int> publishedIndex { 0 };

        std::vector<Loris::Oscillator> oscillators;
        std::vector<int> oscillatorSourceIds;
        std::unordered_map<int, size_t> sourceIdToSlot;

        std::vector<int> lastSnapshotIds;
        std::vector<int> slotForTarget;

        std::vector<uint32_t> referencedStamp;
        uint32_t currentStamp = 0;

        std::vector<double> scratchBuffer;

        std::array<std::array<double, MorphEngine::numSlots>, 2> attackPos {};
        std::array<std::array<double, MorphEngine::numSlots>, 2> attackAge {};
        std::array<std::array<bool,   MorphEngine::numSlots>, 2> attackOn {};
        std::array<int, MorphEngine::numSlots> attackHead {};
        std::array<size_t, MorphEngine::numSlots> attackNext {};
    };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void startControlThread();
    void stopControlThread();
    void controlLoop();
    void controlTick();

    std::thread controlThread;
    std::atomic<bool> controlThreadRunning { false };

    struct MidiNoteEvent
    {
        bool noteOn;
        int noteNumber;
        float velocity = 1.0f;
    };
    std::mutex midiMutex;
    std::deque<MidiNoteEvent> midiQueue;

    std::atomic<float> outputPeak { 0.0f };

    void noteOn (int midiNoteNumber, float velocity = 1.0f);
    void noteOff (int midiNoteNumber);

    void refreshAttackLayers();
    std::atomic<double> attackPreRollSec { 0.001 };

    BorealAnalyzeQueue analyzeQueue;

    std::array<juce::String, MorphEngine::numSlots> slotSourceAudio;

    static constexpr double maxAnalyzedSourceSeconds = 60.0;

    boreal::analyzer::Settings buildAnalyzerSettings() const;

    boreal::analyzer::Settings buildScratchSettings() const;

    juce::File resolveAnalyzerOutputBase (const juce::File& sourceAudio) const;

    juce::File analyzerSourceFile;
    std::vector<double> analyzerSourceSamples;
    double analyzerSourceRate = 0.0;
    std::atomic<int> analyzerSourceGeneration { 0 };
    double analyzerIntervalStart = 0.0;
    double analyzerIntervalEnd = 1.0;
    std::shared_ptr<const boreal::analyzer::PartialFrameData> analyzerFrames;
    std::vector<double> analyzerResynthSamples;
    double analyzerResynthRate = 0.0;
    std::atomic<int> analyzerJobKind { -1 };

    void analyzerJobDelivered (int slotIndex, bool ok,
                               BorealAnalyzeQueue::ResultPtr result, const juce::String& error);

    std::shared_ptr<const std::vector<float>> previewSourceBuf;
    std::shared_ptr<const std::vector<float>> previewResynthBuf;
    juce::SpinLock previewLock;

    std::atomic<bool> previewActive { false };
    std::atomic<int> previewRequestedMode { 0 };
    std::atomic<int> previewRunningMode { 0 };
    std::atomic<double> previewPos { 0.0 };
    std::atomic<bool> previewSnapshotTaken { false };

    double mixerReadPos = 0.0;

    std::atomic<double> previewSourceRate { 0.0 };
    std::atomic<double> previewResynthRate { 0.0 };

    std::atomic<float> previewSrcGain { 1.0f };
    std::atomic<float> previewRsynGain { 1.0f };

public:

    void renderPreview (float* const* channelData, int numChannels, int numFrames);
private:

    MorphEngine morphEngine;

    std::unique_ptr<BorealPresetManager> presetManager;

    juce::ListenerList<SlotLoadListener> slotLoadListeners;

    std::atomic<double> timeIndexDisplay { -1.0 };
    std::array<std::atomic<float>, MorphEngine::numSlots> slotPhaseDisplay { -1.0f, -1.0f, -1.0f, -1.0f };

    std::atomic<bool> crtEnabled { true };
    std::atomic<int> crtStrength { 2 };

    juce::ValueTree guiState { juce::Identifier ("BorealGUI") };

    std::array<Voice, maxVoices> voices;
    uint32_t noteOnCounter = 0;

    std::atomic<int> currentVoices { 1 };

    std::atomic<bool> pitchLockEnabled { false };

    std::atomic<bool> legatoEnabled { false };
    std::atomic<float> glideTimeMs { 0.0f };

    double smoothedPadX = 0.5;
    double smoothedPadY = 0.5;

    std::vector<int> monoNoteStack;

    std::atomic<double> referenceFreqHz { 0.0 };

    double currentSampleRate = 44100.0;

    double pitchBendSemitones = 0.0;

    std::shared_ptr<const AttackLayerSet> publishedAttackLayers;
    juce::SpinLock attackLayersLock;

    void publishAttackLayers (std::shared_ptr<const AttackLayerSet> p)
    {
        const juce::SpinLock::ScopedLockType lock (attackLayersLock);
        publishedAttackLayers = std::move (p);
    }
    std::shared_ptr<const AttackLayerSet> acquireAttackLayers()
    {
        const juce::SpinLock::ScopedLockType lock (attackLayersLock);
        return publishedAttackLayers;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealAudioProcessor)
};
