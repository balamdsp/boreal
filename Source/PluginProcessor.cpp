#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <limits>

#include <juce_dsp/juce_dsp.h>

#include "Helpers/BorealParameters.h"
#include "Managers/BorealPresetManager.h"

namespace
{

    constexpr int morphUpdateHz = 120;

    struct SlotCursorParams
    {
        double rateMult = 1.0;
        double offset = 0.0;
        double w0 = 0.0;
        double w1 = 1.0;
        bool reverse = false;
        bool loop = true;
    };

    struct CursorStep
    {
        double pos = 0.0;
        bool wrapped = false;
        bool rising = true;
    };

    CursorStep advanceSlotCursor (double pos, double sharedDelta,
                                  const SlotCursorParams& p,
                                  bool loop, bool forwardOnly,
                                  double ownDuration, double refDuration, bool asyncClock,
                                  bool rising)
    {
        const double norm = ownDuration > 0.0
            ? (asyncClock ? p.rateMult / ownDuration
                          : p.rateMult * refDuration / ownDuration) : 0.0;
        const double step = sharedDelta * norm * (p.reverse ? -1.0 : 1.0);
        const double range = std::max (1.0e-9, p.w1 - p.w0);
        double np = pos + step;
        bool wrapped = false;
        bool newRising = rising;

        if (loop)
        {
            if (forwardOnly && step >= 0.0)
            {
                if (np >= p.w1)
                {
                    np = p.w0 + std::fmod (np - p.w0, range);
                    wrapped = true;
                }
                else if (np < p.w0)
                {
                    np = p.w0;
                }
            }
            else if (forwardOnly)
            {
                if (np < p.w0)
                {
                    np = p.w1 + std::fmod (np - p.w0, range);
                    wrapped = true;
                }
                else if (np > p.w1)
                {
                    np = p.w1;
                }
            }
            else
            {
                const double mag = step >= 0.0 ? step : -step;
                const double rateSign = sharedDelta >= 0.0 ? 1.0 : -1.0;
                np = pos + (rising ? 1.0 : -1.0) * rateSign * mag;
                if (np >= p.w1 || np <= p.w0)
                {
                    np = (np >= p.w1) ? 2.0 * p.w1 - np : 2.0 * p.w0 - np;
                    newRising = ! rising;
                }
            }
        }
        else
        {
            np = juce::jlimit (p.w0, p.w1, np);
        }

        return { np, wrapped, newRising };
    }

}

BorealAudioProcessor::BorealAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    const juce::String outputGainID = Boreal::PARAMETERS<float>[Boreal::Parameters::OutputGain].ID;
    gainParam = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (outputGainID));

    guiState.setProperty ("voices", 1, nullptr);
    guiState.setProperty ("timeScrub", true, nullptr);
    guiState.setProperty ("loop", true, nullptr);
    guiState.setProperty ("pitchLock", false, nullptr);
    guiState.setProperty ("forwardOnly", false, nullptr);

    voicesValue    = guiState.getPropertyAsValue ("voices", nullptr);
    timeScrubValue = guiState.getPropertyAsValue ("timeScrub", nullptr);
    loopValue      = guiState.getPropertyAsValue ("loop", nullptr);
    pitchLockValue = guiState.getPropertyAsValue ("pitchLock", nullptr);
    forwardOnlyValue = guiState.getPropertyAsValue ("forwardOnly", nullptr);

    guiState.setProperty ("analyzerResolutionHz", 40.0, nullptr);
    guiState.setProperty ("analyzerAmpFloorDb", -60.0, nullptr);
    if (! guiState.hasProperty ("analyzerCleanPartials"))
    {
        juce::var legacy = guiState.getProperty ("analyzerPipeline");
        guiState.setProperty ("analyzerCleanPartials",
                              legacy.isVoid() ? false : static_cast<bool> (legacy), nullptr);
    }
    if (! guiState.hasProperty ("analyzerOnsetSensitivity"))
    {
        juce::var legacy = guiState.getProperty ("analyzerOnsetMult");
        const double sens = legacy.isVoid()
            ? boreal::analyzer::Settings::kOnsetSensDefault
            : boreal::analyzer::Settings::onsetMultToSensitivity ((double) legacy);
        guiState.setProperty ("analyzerOnsetSensitivity", sens, nullptr);
    }
    guiState.setProperty ("analyzerPreRollMs", 1.0, nullptr);

    analyzerResolutionValue     = guiState.getPropertyAsValue ("analyzerResolutionHz", nullptr);
    analyzerAmpFloorValue       = guiState.getPropertyAsValue ("analyzerAmpFloorDb", nullptr);
    analyzerCleanPartialsValue  = guiState.getPropertyAsValue ("analyzerCleanPartials", nullptr);
    analyzerOnsetSensitivityValue = guiState.getPropertyAsValue ("analyzerOnsetSensitivity", nullptr);
    analyzerPreRollMsValue      = guiState.getPropertyAsValue ("analyzerPreRollMs", nullptr);

    guiState.setProperty ("tooltipDelayMs", 700, nullptr);
    guiState.setProperty ("tooltipsEnabled", true, nullptr);
    guiState.setProperty ("confirmDestructive", true, nullptr);

    tooltipDelayMsValue     = guiState.getPropertyAsValue ("tooltipDelayMs", nullptr);
    tooltipsEnabledValue    = guiState.getPropertyAsValue ("tooltipsEnabled", nullptr);
    confirmDestructiveValue = guiState.getPropertyAsValue ("confirmDestructive", nullptr);

    guiState.setProperty ("uiMode", "performance", nullptr);
    guiState.setProperty ("analyzerWindowWidthHz", 80.0, nullptr);
    guiState.setProperty ("analyzerLoCutHz", 20.0, nullptr);
    guiState.setProperty ("analyzerHiCutHz", 20000.0, nullptr);
    guiState.setProperty ("analyzerFreqDriftHz", 40.0, nullptr);
    guiState.setProperty ("analyzerNoiseWidthHz", 500.0, nullptr);
    guiState.setProperty ("analyzerFundamentalHz", 0.0, nullptr);
    guiState.setProperty ("previewSrcVolDb", 0.0, nullptr);
    guiState.setProperty ("previewRsynVolDb", 0.0, nullptr);

    uiModeValue                 = guiState.getPropertyAsValue ("uiMode", nullptr);
    analyzerWindowWidthHzValue  = guiState.getPropertyAsValue ("analyzerWindowWidthHz", nullptr);
    analyzerLoCutHzValue        = guiState.getPropertyAsValue ("analyzerLoCutHz", nullptr);
    analyzerHiCutHzValue        = guiState.getPropertyAsValue ("analyzerHiCutHz", nullptr);
    analyzerFreqDriftHzValue    = guiState.getPropertyAsValue ("analyzerFreqDriftHz", nullptr);
    analyzerNoiseWidthHzValue   = guiState.getPropertyAsValue ("analyzerNoiseWidthHz", nullptr);
    analyzerFundamentalHzValue  = guiState.getPropertyAsValue ("analyzerFundamentalHz", nullptr);
    previewSrcVolDbValue        = guiState.getPropertyAsValue ("previewSrcVolDb", nullptr);
    previewRsynVolDbValue       = guiState.getPropertyAsValue ("previewRsynVolDb", nullptr);

    analyzeQueue.configure (
        [this] (int slotIndex, const juce::File& sdifFile) { loadSlotAsync (slotIndex, sdifFile); },
        [this] (int slotIndex, const juce::String& errorMessage)
        {
            slotLoadListeners.call ([&] (SlotLoadListener& l)
            {
                l.slotLoadFinished (slotIndex, false, errorMessage);
            });
        },
        [this] (int slotIndex, bool ok, const juce::String& errorMessage)
        {
            slotLoadListeners.call ([&] (SlotLoadListener& l)
            {
                l.slotLoadFinished (slotIndex, ok, errorMessage);
            });
        },
        [this] (int slotIndex, bool ok, BorealAnalyzeQueue::ResultPtr result, const juce::String& errorMessage)
        {
            analyzerJobDelivered (slotIndex, ok, std::move (result), errorMessage);
        });

    for (auto& voice : voices)
    {
        voice.buffers[0] = std::make_shared<MorphSnapshot>();
        voice.buffers[1] = std::make_shared<MorphSnapshot>();
    }

    presetManager = std::make_unique<BorealPresetManager> (this);

    crtEnabled = presetManager->getCrtEnabled();
    crtStrength = presetManager->getCrtStrength();

    startControlThread();
}

BorealAudioProcessor::~BorealAudioProcessor()
{
    stopControlThread();
}

juce::AudioProcessorValueTreeState::ParameterLayout BorealAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    for (const auto& entry : Boreal::PARAMETERS<float>)
    {
        const Boreal::Parameter<float>& p = entry.second;

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p.ID, 1 },
            p.label,
            juce::NormalisableRange<float> (p.min_value, p.max_value),
            p.default_value));
    }

    juce::StringArray scaleChoices;
    for (const auto s : Boreal::Zoom::ZOOM_PERCENTS)
        scaleChoices.add (juce::String (s, 0) + "%");

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { Boreal::Zoom::UI_SCALE_ID, 1 },
        "UI Scale", scaleChoices, Boreal::Zoom::UI_SCALE_DEFAULT));

    return { params.begin(), params.end() };
}

void BorealAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const int safeBlock = juce::jmax (1, samplesPerBlock);

    for (auto& voice : voices)
    {
        voice.timeIndexPhase = 0.0;
        voice.timeIndexRising = true;
        voice.timeIndexSign = 0;
        for (auto& cursor : voice.slotCursors)
        {
            cursor.phase = 0.0;
            cursor.rising = true;
        }
        voice.gateOn = false;
        voice.noteNumber = -1;
        voice.glideFactor = -1.0;
        monoNoteStack.clear();

        voice.envelope.setSampleRate (sampleRate);
        voice.envelope.reset();
        voice.oscillators.clear();
        voice.oscillatorSourceIds.clear();
        voice.sourceIdToSlot.clear();
        voice.lastSnapshotIds.clear();
        voice.slotForTarget.clear();
        voice.referencedStamp.clear();
        voice.currentStamp = 0;
        voice.scratchBuffer.assign ((size_t) safeBlock, 0.0);

        for (int h = 0; h < 2; ++h)
        {
            voice.attackPos[(size_t) h].fill (0.0);
            voice.attackAge[(size_t) h].fill (0.0);
            voice.attackOn[(size_t) h].fill (false);
        }
        voice.attackHead.fill (0);
        voice.attackNext.fill (0);
    }
}

void BorealAudioProcessor::releaseResources() {}

void BorealAudioProcessor::refreshAttackLayers()
{

    auto layers = std::make_shared<AttackLayerSet>();

    for (int i = 0; i < MorphEngine::numSlots; ++i)
    {
        const MorphEngine::AttackLayerInfo info = morphEngine.getAttackLayer (i);
        const MorphEngine::SlotInfo meta = morphEngine.getSlotInfo (i);
        auto& dst = layers->slots[(size_t) i];

        if (! info.present || info.sampleRate <= 0.0 || info.samples.empty()
            || info.onsetTimes.empty() || meta.durationSeconds <= 0.0)
            continue;

        dst.present = true;
        dst.sampleRate = info.sampleRate;
        dst.durationSec = meta.durationSeconds;
        dst.loudnessGain = meta.loudnessGain;
        dst.samples = std::move (info.samples);
        dst.onsets = std::move (info.onsetTimes);

        const double lenSec = (double) dst.samples.size() / dst.sampleRate;
        if (lenSec + 1.0e-4 < 0.75 * dst.durationSec)
        {
            dst.legacySnippet = true;
            dst.posOffset = (dst.onsets.front() - attackPreRollSec.load()) * dst.sampleRate;
            dst.onsets.assign (1, dst.onsets.front());
        }
    }

    publishAttackLayers (layers);
}

void BorealAudioProcessor::loadSlotAsync (int slotIndex, const juce::File& sdifFile)
{

    juce::Thread::launch ([this, slotIndex, sdifFile]
    {
        juce::String errorMessage;
        const bool ok = morphEngine.loadSlot (slotIndex, sdifFile, errorMessage);

        refreshAttackLayers();

        if (ok || errorMessage.isNotEmpty())
            juce::MessageManager::callAsync ([this, slotIndex, ok, errorMessage]
            {
                slotLoadListeners.call ([&] (SlotLoadListener& l)
                {
                    l.slotLoadFinished (slotIndex, ok, errorMessage);
                });
            });
    });
}

void BorealAudioProcessor::setCrtEnabled (bool b)
{
    crtEnabled = b;
    if (presetManager != nullptr)
        presetManager->setCrtEnabled (b);
}

void BorealAudioProcessor::setCrtStrength (int strength)
{
    crtStrength = jlimit (0, 2, strength);
    if (presetManager != nullptr)
        presetManager->setCrtStrength (crtStrength.load());
}

void BorealAudioProcessor::resetAnalyzerDefaults()
{
    guiState.setProperty ("analyzerResolutionHz", 40.0, nullptr);
    guiState.setProperty ("analyzerAmpFloorDb", -60.0, nullptr);
    guiState.setProperty ("analyzerCleanPartials", false, nullptr);
    guiState.setProperty ("analyzerOnsetSensitivity",
                          boreal::analyzer::Settings::kOnsetSensDefault, nullptr);
    guiState.setProperty ("analyzerPreRollMs", 1.0, nullptr);
    guiState.setProperty ("analyzerWindowWidthHz", 80.0, nullptr);
    guiState.setProperty ("analyzerLoCutHz", 20.0, nullptr);
    guiState.setProperty ("analyzerHiCutHz", 20000.0, nullptr);
    guiState.setProperty ("analyzerFreqDriftHz", 40.0, nullptr);
    guiState.setProperty ("analyzerNoiseWidthHz", 500.0, nullptr);
    guiState.setProperty ("analyzerFundamentalHz", 0.0, nullptr);
    guiState.setProperty ("previewSrcVolDb", 0.0, nullptr);
    guiState.setProperty ("previewRsynVolDb", 0.0, nullptr);
    attackPreRollSec.store (0.001);
}

void BorealAudioProcessor::analyzeAndLoadAsync (int slotIndex, const juce::File& audioFile)
{

    jassert (slotIndex >= 0 && slotIndex < MorphEngine::numSlots);

    BorealAnalyzeQueue::Request request;
    request.slotIndex = slotIndex;
    request.sourceAudio = audioFile;
    request.outputBase = resolveAnalyzerOutputBase (audioFile);
    request.settings = buildAnalyzerSettings();

    slotSourceAudio[(size_t) slotIndex] = audioFile.getFullPathName();

    analyzeQueue.enqueue (request);
}

boreal::analyzer::Settings BorealAudioProcessor::buildAnalyzerSettings() const
{
    boreal::analyzer::Settings s;

    s.resolution = juce::jlimit (20.0, 2000.0, getAnalyzerResolutionHz());
    s.ampFloor   = juce::jlimit (-180.0, -20.0, getAnalyzerAmpFloorDb());
    s.cleanPartials = getAnalyzerCleanPartials();
    s.onsetMult  = juce::jlimit (0.8, 4.0, getAnalyzerOnsetMult());

    s.windowWidth = juce::jlimit (50.0, 4000.0, getAnalyzerWindowWidthHz());
    const double loCut = getAnalyzerLoCutHz();
    s.freqFloor = (loCut > 0.0) ? juce::jlimit (20.0, 2000.0, loCut) : 0.0;
    s.hiCutHz = (getAnalyzerHiCutHz() > 0.0) ? juce::jlimit (500.0, 22000.0, getAnalyzerHiCutHz()) : 0.0;
    const double drift = getAnalyzerFreqDriftHz();
    s.freqDrift = (drift > 0.0) ? juce::jlimit (1.0, 2000.0, drift) : 0.0;
    s.bwWidth = juce::jlimit (100.0, 8000.0, getAnalyzerNoiseWidthHz());

    if (s.cleanPartials && getAnalyzerFundamentalHz() > 0.0)
    {
        const double f = juce::jlimit (30.0, 2000.0, getAnalyzerFundamentalHz());
        s.plLower = 0.9 * f;
        s.plUpper = 1.15 * f;
    }

    s.maxDurationSeconds = maxAnalyzedSourceSeconds;
    s.writeAttackSidecar = true;

    return s;
}

juce::File BorealAudioProcessor::resolveAnalyzerOutputBase (const juce::File& sourceAudio) const
{

    const juce::File dir = sourceAudio.getParentDirectory();
    if (dir.isDirectory() && dir.hasWriteAccess())
        return dir.getChildFile (sourceAudio.getFileNameWithoutExtension());

    const juce::File fallbackDir (getDefaultAnalyzerDirectory());
    fallbackDir.createDirectory();
    return fallbackDir.getChildFile (sourceAudio.getFileNameWithoutExtension());
}

juce::String BorealAudioProcessor::slotAnalyzeStageText() const
{
    const int idx = analyzeQueue.currentStageIndex();
    if (idx < 0)
        return {};
    return boreal::analyzer::stageName ((boreal::analyzer::Stage) idx);
}

juce::String BorealAudioProcessor::getAnalyzerSourceName() const
{
    return analyzerSourceFile.existsAsFile() ? analyzerSourceFile.getFileName() : juce::String();
}

namespace
{
    bool decodeAudioFileJUCE (const juce::File& f, std::vector<double>& mono,
                              double& rate, std::string& err)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
        if (reader == nullptr)
        {
            err = "unsupported format";
            return false;
        }

        const int64_t length = reader->lengthInSamples;
        if (length <= 0)
        {
            err = "no audio in file";
            return false;
        }

        juce::AudioBuffer<float> buffer ((int) juce::jmin ((int64_t) reader->numChannels, (int64_t) 2),
                                         (int) juce::jmin (length, (int64_t) 1 << 28));
        reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);

        rate = reader->sampleRate;
        mono.resize ((size_t) buffer.getNumSamples());
        const int chans = buffer.getNumChannels();
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            double sum = 0.0;
            for (int ch = 0; ch < chans; ++ch)
                sum += buffer.getSample (ch, i);
            mono[(size_t) i] = sum / (double) chans;
        }
        return true;
    }
}

juce::String BorealAudioProcessor::openAnalyzerSource (const juce::File& audioFile)
{

    std::vector<double> mono;
    double rate = 0.0;
    std::string err;

    boreal::analyzer::Settings probe;
    probe.maxDurationSeconds = maxAnalyzedSourceSeconds;

    const juce::String ext = audioFile.getFileExtension().toLowerCase();
    const bool native = ext == ".wav" || ext == ".wave" || ext == ".aif" || ext == ".aiff";

    const bool ok = native
        ? boreal::analyzer::readAudioFile (audioFile.getFullPathName().toStdString(), mono, rate, err)
        : decodeAudioFileJUCE (audioFile, mono, rate, err);

    if (! ok)
        return "couldn't read " + audioFile.getFileName() + ": " + juce::String (err);
    if (mono.empty() || rate <= 0.0)
        return "no audio in " + audioFile.getFileName();

    if (probe.maxDurationSeconds > 0.0 && (double) mono.size() / rate > probe.maxDurationSeconds)
    {
        const size_t capped = (size_t) (probe.maxDurationSeconds * rate);
        mono.resize (capped);
    }

    analyzerSourceFile = audioFile;
    analyzerSourceSamples = std::move (mono);
    analyzerSourceRate = rate;
    analyzerIntervalStart = 0.0;
    analyzerIntervalEnd = 1.0;
    analyzerSourceGeneration.fetch_add (1);
    std::atomic_store (&analyzerFrames, std::shared_ptr<const boreal::analyzer::PartialFrameData> {});
    analyzerResynthSamples.clear();
    analyzerResynthRate = 0.0;
    analyzerJobKind.store (-1);

    previewStop();

    auto buf = std::make_shared<std::vector<float>> (analyzerSourceSamples.size());

    float decPeak = 0.0f;
    for (double s : analyzerSourceSamples)
        decPeak = juce::jmax (decPeak, (float) std::abs (s));
    const float norm = decPeak > 0.0f ? (float) (0.99 / decPeak) : 1.0f;

    for (size_t i = 0; i < buf->size(); ++i)
        (*buf)[i] = (float) analyzerSourceSamples[i] * norm;
    {
        const juce::SpinLock::ScopedLockType lock (previewLock);
        previewSourceBuf = std::move (buf);
    }
    previewSourceRate.store (analyzerSourceRate);

    return {};
}

void BorealAudioProcessor::clearAnalyzerSession()
{
    previewStop();

    analyzerSourceFile = juce::File();
    analyzerSourceSamples.clear();
    analyzerSourceRate = 0.0;
    analyzerIntervalStart = 0.0;
    analyzerIntervalEnd = 1.0;
    analyzerSourceGeneration.fetch_add (1);
    std::atomic_store (&analyzerFrames, std::shared_ptr<const boreal::analyzer::PartialFrameData> {});
    analyzerResynthSamples.clear();
    analyzerResynthRate = 0.0;
    analyzerJobKind.store (-1);

    {
        const juce::SpinLock::ScopedLockType lock (previewLock);
        previewSourceBuf.reset();
        previewResynthBuf.reset();
    }
    previewSourceRate.store (0.0);
    previewResynthRate.store (0.0);
}

void BorealAudioProcessor::refreshScratchOnsets()
{
    if (! hasAnalyzerSource())
        return;

    const auto frames = getAnalyzerFrames();
    if (frames == nullptr || frames->times.empty())
        return;

    const std::vector<double> onsets = boreal::analyzer::detectOnsets (
        analyzerSourceSamples, analyzerSourceRate, getAnalyzerOnsetMult());

    auto updated = std::make_shared<boreal::analyzer::PartialFrameData> (*frames);
    updated->onsets = onsets;
    std::atomic_store (&analyzerFrames,
                       std::shared_ptr<const boreal::analyzer::PartialFrameData> (std::move (updated)));
}

boreal::analyzer::Settings BorealAudioProcessor::buildScratchSettings() const
{
    boreal::analyzer::Settings s = buildAnalyzerSettings();
    s.writeFiles = false;
    s.writeAttackSidecar = false;
    s.keepPartialData = true;
    s.regionStart = analyzerIntervalStart;
    s.regionEnd = analyzerIntervalEnd;
    return s;
}

void BorealAudioProcessor::analyzeScratch()
{
    if (! hasAnalyzerSource())
        return;
    if (analyzeQueue.isBusy())
        return;

    BorealAnalyzeQueue::Request request;
    request.slotIndex = -1;
    request.kind = BorealAnalyzeQueue::Kind::Analyze;
    request.sourceAudio = analyzerSourceFile;
    request.outputBase = juce::File::getCurrentWorkingDirectory().getChildFile ("_unused");
    request.settings = buildScratchSettings();
    request.autoLoad = false;
    request.deliverResult = true;

    analyzerResynthSamples.clear();
    analyzerResynthRate = 0.0;
    previewStop();
    analyzerJobKind.store (0);
    analyzeQueue.enqueue (request);
}

void BorealAudioProcessor::synthesizeScratch()
{
    const auto frames = getAnalyzerFrames();
    if (frames == nullptr || frames->times.empty())
        return;
    if (analyzeQueue.isBusy())
        return;

    BorealAnalyzeQueue::Request request;
    request.slotIndex = -1;
    request.kind = BorealAnalyzeQueue::Kind::Synthesize;
    request.synthInput = *frames;
    request.synthSampleRate = analyzerSourceRate > 0.0 ? analyzerSourceRate : 44100.0;
    request.deliverResult = true;

    previewStop();
    analyzerJobKind.store (1);
    analyzeQueue.enqueue (request);
}

void BorealAudioProcessor::sendScratchToSlot (int slotIndex)
{
    if (! hasAnalyzerSource())
        return;
    jassert (slotIndex >= 0 && slotIndex < MorphEngine::numSlots);

    BorealAnalyzeQueue::Request request;
    request.slotIndex = slotIndex;
    request.kind = BorealAnalyzeQueue::Kind::Analyze;
    request.sourceAudio = analyzerSourceFile;
    request.outputBase = resolveAnalyzerOutputBase (analyzerSourceFile);
    request.settings = buildAnalyzerSettings();
    request.settings.keepPartialData = true;
    request.settings.regionStart = analyzerIntervalStart;
    request.settings.regionEnd = analyzerIntervalEnd;
    request.autoLoad = true;
    request.deliverResult = true;

    slotSourceAudio[(size_t) slotIndex] = analyzerSourceFile.getFullPathName();
    analyzerJobKind.store (0);
    analyzeQueue.enqueue (request);
}

bool BorealAudioProcessor::exportScratchWav (const juce::File& outFile)
{
    if (analyzerResynthSamples.empty() || analyzerResynthRate <= 0.0)
        return false;
    return boreal::analyzer::writeWavMono16 (outFile.getFullPathName().toStdString(),
                                             analyzerResynthRate, analyzerResynthSamples);
}

void BorealAudioProcessor::exportScratchSdifAsync (const juce::File& destDirectory)
{
    if (! hasAnalyzerSource() || ! destDirectory.isDirectory() || ! destDirectory.hasWriteAccess())
        return;

    BorealAnalyzeQueue::Request request;
    request.slotIndex = -1;
    request.kind = BorealAnalyzeQueue::Kind::Analyze;
    request.sourceAudio = analyzerSourceFile;
    request.outputBase = destDirectory.getChildFile (analyzerSourceFile.getFileNameWithoutExtension());
    request.settings = buildAnalyzerSettings();
    request.settings.keepPartialData = true;
    request.settings.regionStart = analyzerIntervalStart;
    request.settings.regionEnd = analyzerIntervalEnd;
    request.autoLoad = false;
    request.deliverResult = true;

    analyzerJobKind.store (0);
    analyzeQueue.enqueue (request);
}

void BorealAudioProcessor::analyzerJobDelivered (int slotIndex, bool ok,
                                                 BorealAnalyzeQueue::ResultPtr result,
                                                 const juce::String& error)
{
    const int kind = analyzerJobKind.exchange (-1);

    if (! ok || result == nullptr)
    {
        DBG ("Analyzer window job failed: " << error);
        return;
    }

    if (result->partials.times.empty() == false)
        std::atomic_store (&analyzerFrames,
                           std::shared_ptr<const boreal::analyzer::PartialFrameData> (
                               new boreal::analyzer::PartialFrameData (result->partials)));

    if (kind == 1 || ! result->renderedSamples.empty())
    {
        analyzerResynthSamples = result->renderedSamples;
        analyzerResynthRate = result->renderedSampleRate;

        auto buf = std::make_shared<std::vector<float>> (analyzerResynthSamples.size());
        for (size_t i = 0; i < buf->size(); ++i)
            (*buf)[i] = (float) analyzerResynthSamples[i];
        {
            const juce::SpinLock::ScopedLockType lock (previewLock);
            previewResynthBuf = std::move (buf);
        }
        previewResynthRate.store (analyzerResynthRate > 0.0 ? analyzerResynthRate : 44100.0);
    }

    ignoreUnused (slotIndex, error);
}

bool BorealAudioProcessor::previewPlay (int mode)
{
    {
        const juce::SpinLock::ScopedLockType lock (previewLock);
        if ((mode == previewSource && ! previewSourceBuf)
            || (mode == previewResynth && ! previewResynthBuf))
            return false;
    }
    previewSnapshotTaken.store (false);
    previewRequestedMode.store (mode);
    previewRunningMode.store (mode);
    previewActive.store (true);
    return true;
}

void BorealAudioProcessor::previewStop()
{
    previewActive.store (false);
    previewRequestedMode.store (previewOff);
    previewRunningMode.store (previewOff);
}

void BorealAudioProcessor::renderPreview (float* const* channelData, int numChannels, int numFrames)
{
    if (! previewActive.load (std::memory_order_relaxed))
        return;
    if (numFrames <= 0 || numChannels <= 0)
        return;

    std::shared_ptr<const std::vector<float>> buf;
    float gain = 1.0f;
    switch (previewRunningMode.load (std::memory_order_relaxed))
    {
        case previewSource:
            {
                const juce::SpinLock::ScopedLockType lock (previewLock);
                buf = previewSourceBuf;
            }
            gain = previewSrcGain.load();
            break;
        case previewResynth:
            {
                const juce::SpinLock::ScopedLockType lock (previewLock);
                buf = previewResynthBuf;
            }
            gain = previewRsynGain.load();
            break;
        default:
            return;
    }

    if (buf == nullptr || buf->empty() || numChannels <= 0)
    {
        previewActive.store (false);
        return;
    }

    const float* samples = buf->data();
    const size_t total = buf->size();

    if (! previewSnapshotTaken.exchange (true))
        mixerReadPos = 0.0;

    for (int f = 0; f < numFrames; ++f)
    {
        const size_t idx = (size_t) mixerReadPos;
        if (idx >= total)
        {
            previewActive.store (false);
            previewPos.store ((double) total / previewSourceRate.load());
            return;
        }
        const float v = samples[idx] * gain;
        for (int c = 0; c < numChannels; ++c)
            channelData[c][f] += v;
        ++mixerReadPos;
    }

    previewPos.store (mixerReadPos, std::memory_order_relaxed);
}

void BorealAudioProcessor::startControlThread()
{
    if (controlThreadRunning.exchange (true))
        return;

    controlThread = std::thread (&BorealAudioProcessor::controlLoop, this);
}

void BorealAudioProcessor::stopControlThread()
{
    if (! controlThreadRunning.exchange (false))
        return;

    if (controlThread.joinable())
        controlThread.join();
}

void BorealAudioProcessor::controlLoop()
{
    const auto interval = std::chrono::microseconds (1000000 / morphUpdateHz);

    while (controlThreadRunning.load())
    {
        controlTick();
        std::this_thread::sleep_for (interval);
    }
}

void BorealAudioProcessor::controlTick()
{

    currentVoices.store (getCurrentVoices());
    pitchLockEnabled.store (getPitchLockEnabled());
    legatoEnabled.store (getLegatoEnabled());
    glideTimeMs.store (Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::glide_time_ms].ID, 0.0f));

    previewSrcGain.store (juce::Decibels::decibelsToGain<float> (
        juce::jlimit (-40.0f, 6.0f, (float) previewSrcVolDbValue.getValue())));
    previewRsynGain.store (juce::Decibels::decibelsToGain<float> (
        juce::jlimit (-40.0f, 6.0f, (float) previewRsynVolDbValue.getValue())));

    const int numVoices = juce::jlimit (1, maxVoices, currentVoices.load());

    if (numVoices != 1 || ! legatoEnabled.load())
        monoNoteStack.clear();

    {
        std::deque<MidiNoteEvent> pending;
        {
            std::lock_guard<std::mutex> lock (midiMutex);
            pending.swap (midiQueue);
        }
        for (const auto& ev : pending)
        {
            if (ev.noteOn)
                noteOn (ev.noteNumber, ev.velocity);
            else
                noteOff (ev.noteNumber);
        }
    }

    const float rawPadX = Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::freqs_interp_factor].ID, 0.5f);
    const float rawPadY = Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::mags_interp_factor].ID, 0.5f);

    double padAlpha = 1.0;
    const float padSmoothMs = Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::pad_smoothing_ms].ID, 0.0f);
    if (padSmoothMs > 0.0f)
    {
        const double tau = (double) padSmoothMs / 1000.0;
        padAlpha = 1.0 - std::exp (-1.0 / ((double) morphUpdateHz * tau));
    }
    smoothedPadX += padAlpha * ((double) rawPadX - smoothedPadX);
    smoothedPadY += padAlpha * ((double) rawPadY - smoothedPadY);

    const float x = (float) smoothedPadX;
    const float y = (float) smoothedPadY;

    const float offsetFreq = Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::morph_freq_x].ID, 0.0f);
    const float offsetAmp  = Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::morph_amp_x].ID, 0.0f);
    const float offsetBw   = Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::morph_bw_x].ID, 0.0f);
    const bool crossMode = Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::cross_mode].ID, 0.0f) >= 0.5f;
    const int crossFreqCorner = juce::jlimit (0, 3, juce::roundToInt (
        Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::cross_freq_corner].ID, 0.0f)));
    const int crossAmpCorner = juce::jlimit (0, 3, juce::roundToInt (
        Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::cross_amp_corner].ID, 3.0f)));
    auto crossCornerX = [] (int c) { return (c == 1 || c == 3) ? 1.0f : 0.0f; };
    const float xFreq = crossMode ? crossCornerX (crossFreqCorner)
                                  : juce::jlimit (0.0f, 1.0f, x + offsetFreq);
    const float xAmp  = crossMode ? crossCornerX (crossAmpCorner)
                                  : juce::jlimit (0.0f, 1.0f, x + offsetAmp);
    const float xBw   = crossMode ? crossCornerX (crossAmpCorner)
                                  : juce::jlimit (0.0f, 1.0f, x + offsetBw);

    referenceFreqHz.store (morphEngine.estimatedReferenceFrequency (xFreq, y));

    float ratePercent = Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::time_scrub_rate].ID, 100.0f);
    if (! getTimeScrubEnabled())
        ratePercent = 100.0f;

    const float axW = crossMode ? xAmp : x;
    const float cornerW[MorphEngine::numSlots] = {
        (1.0f - axW) * y, axW * y, (1.0f - axW) * (1.0f - y), axW * (1.0f - y)
    };

    std::array<SlotCursorParams, MorphEngine::numSlots> cursorParams;
    for (int s = 0; s < MorphEngine::numSlots; ++s)
    {
        const int base = (int) Boreal::Parameters::slot1_rate + s * 5;
        auto& cp = cursorParams[(size_t) s];
        cp.rateMult = (double) Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[base].ID, 100.0f) / 100.0;
        cp.offset = (double) Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[base + 1].ID, 0.0f) / 100.0;
        cp.w0 = (double) Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[base + 2].ID, 0.0f) / 100.0;
        cp.w1 = (double) Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[base + 3].ID, 100.0f) / 100.0;
        if (cp.w1 <= cp.w0 + 1.0e-6) { cp.w0 = 0.0; cp.w1 = 1.0; }
        cp.reverse = Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[base + 4].ID, 0.0f) >= 0.5f;
        cp.loop = Boreal::getParameterValueSafe<float> (
            &apvts,
            Boreal::PARAMETERS<float>[(Boreal::Parameters) ((int) Boreal::Parameters::slot1_loopmode + s)].ID,
            1.0f) >= 0.5f;
    }

    float formantShiftSt[MorphEngine::numSlots] = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (int s = 0; s < MorphEngine::numSlots; ++s)
    {
        formantShiftSt[(size_t) s] = Boreal::getParameterValueSafe<float> (
            &apvts,
            Boreal::PARAMETERS<float>[(Boreal::Parameters) ((int) Boreal::Parameters::slot1_formant + s)].ID,
            0.0f);
    }
    if (crossMode)
    {
        const int crossFormCorner = juce::jlimit (0, 3, juce::roundToInt (
            Boreal::getParameterValueSafe<float> (
                &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::cross_form_corner].ID, 3.0f)));
        const float xForm = crossCornerX (crossFormCorner);
        float wF[MorphEngine::numSlots] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float wSum = 0.0f;
        for (int s = 0; s < MorphEngine::numSlots; ++s)
        {
            if (! morphEngine.isSlotLoaded (s))
                continue;
            wF[(size_t) s] = ((s == 1 || s == 3) ? xForm : 1.0f - xForm)
                           * (s < 2 ? y : 1.0f - y);
            wSum += wF[(size_t) s];
        }
        for (int s = 0; s < MorphEngine::numSlots; ++s)
            formantShiftSt[(size_t) s] *= (wSum > 1.0e-6f ? wF[(size_t) s] / wSum : 0.0f);
    }

    double refDurationSeconds = crossMode ? morphEngine.estimatedRealtimeDuration (xAmp, y)
                                            : morphEngine.estimatedRealtimeDuration (x, y);
    double slotOwnDur[MorphEngine::numSlots] = { 0.0, 0.0, 0.0, 0.0 };
    {
        const int loadedMask = morphEngine.getLoadedMask();
        double longest = 0.0;
        bool any = false;
        for (int s = 0; s < MorphEngine::numSlots; ++s)
        {
            if ((loadedMask & (1 << s)) == 0 || cornerW[(size_t) s] < 1.0e-6f)
                continue;
            const double ownDur = morphEngine.getSlotInfo (s).durationSeconds;
            if (ownDur <= 0.0)
                continue;
            slotOwnDur[(size_t) s] = ownDur;
            const auto& cp = cursorParams[(size_t) s];
            const double eff = cp.rateMult > 1.0e-6
                ? ownDur * (cp.w1 - cp.w0) / cp.rateMult : ownDur;
            longest = juce::jmax (longest, eff);
            any = true;
        }
        if (any && longest > 0.0)
            refDurationSeconds = longest;
    }
    const double rateHz = refDurationSeconds > 0.0
                               ? ((double) ratePercent / 100.0) / refDurationSeconds
                               : 0.0;

    const double step = rateHz / (double) morphUpdateHz;

    {
        auto layers = acquireAttackLayers();
        const int loadedMask = morphEngine.getLoadedMask();
        double weights[MorphEngine::numSlots] = { 0.0, 0.0, 0.0, 0.0 };
        for (int i = 0; i < MorphEngine::numSlots; ++i)
        {
            if ((loadedMask & (1 << i)) == 0)
                continue;
            weights[i] = cornerW[i];
        }
        for (int v = 0; v < maxVoices; ++v)
        {
            const int writeIndex = 1 - voices[(size_t) v].publishedIndex.load();
            auto& snap = *voices[(size_t) v].buffers[(size_t) writeIndex];
            for (int i = 0; i < MorphEngine::numSlots; ++i)
                snap.attackBlendShare[(size_t) i] = (float) weights[i];
        }
    }

    const int rateSign = (step > 0.0) ? 1 : (step < 0.0 ? -1 : 0);
    const double stepMag = (step >= 0.0) ? step : -step;

    const bool loop = getLoopEnabled();

    for (int v = 0; v < maxVoices; ++v)
    {
        auto& voice = voices[(size_t) v];

        if (v >= numVoices)
        {
            if (voice.gateOn)
            {
                voice.gateOn = false;
                voice.envelope.noteOff();
            }
        }

        if (! voice.gateOn && ! voice.envelope.isActive())
        {
            const int writeIndex = 1 - voice.publishedIndex.load();
            voice.buffers[(size_t) writeIndex]->targets.clear();
            voice.publishedIndex.store (writeIndex);
            continue;
        }

        if (rateSign != 0 && rateSign != voice.timeIndexSign)
        {
            voice.timeIndexSign = rateSign;
            voice.timeIndexRising = (rateSign > 0);
        }

        const double prevPhase = voice.timeIndexPhase;

        if (loop)
        {
            if (getForwardOnlyEnabled())
            {

                const double fwd = (step > 0.0) ? step : 0.0;
                voice.timeIndexPhase += fwd;
                if (voice.timeIndexPhase >= 1.0)
                    voice.timeIndexPhase = std::fmod (voice.timeIndexPhase, 1.0);
            }
            else
            {

                voice.timeIndexPhase += voice.timeIndexRising ? stepMag : -stepMag;
                if (voice.timeIndexPhase >= 1.0)
                {
                    voice.timeIndexPhase = 2.0 - voice.timeIndexPhase;
                    voice.timeIndexRising = false;
                }
                else if (voice.timeIndexPhase <= 0.0)
                {
                    voice.timeIndexPhase = -voice.timeIndexPhase;
                    voice.timeIndexRising = true;
                }
            }
        }
            else
            {

                voice.timeIndexPhase += step;
                if (voice.timeIndexPhase >= 1.0)
                {
                    voice.timeIndexPhase = 1.0;
                    if (voice.gateOn)
                    {
                        voice.gateOn = false;
                        voice.envelope.noteOff();
                    }
                }
                else if (voice.timeIndexPhase <= 0.0)
                {
                    voice.timeIndexPhase = 0.0;
                }
            }

            double slotPrev[MorphEngine::numSlots];
            double slotNew[MorphEngine::numSlots];
            {
                const bool fwdOnly = getForwardOnlyEnabled();
                const bool async = loop;
                const double ownBase = ((double) ratePercent / 100.0) / (double) morphUpdateHz;
                double slotBase = voice.timeIndexPhase - prevPhase;
                if (loop && fwdOnly)
                {
                    const double fwd = (step > 0.0) ? step : 0.0;
                    slotBase = fwd;
                }
                for (int s = 0; s < MorphEngine::numSlots; ++s)
                {
                    slotPrev[(size_t) s] = voice.slotCursors[(size_t) s].phase;
                    const double baseDelta = async ? ownBase : slotBase;
                    const CursorStep cs = advanceSlotCursor (slotPrev[(size_t) s], baseDelta,
                                                             cursorParams[(size_t) s],
                                                             loop && cursorParams[(size_t) s].loop,
                                                             fwdOnly,
                                                             slotOwnDur[(size_t) s],
                                                             refDurationSeconds, async,
                                                             voice.slotCursors[(size_t) s].rising);
                    voice.slotCursors[(size_t) s].phase = cs.pos;
                    voice.slotCursors[(size_t) s].rising = cs.rising;
                    slotNew[(size_t) s] = cs.pos;
                    if (cs.wrapped)
                        voice.attackNext[(size_t) s] = 0;
                }
            }

            {
                auto layers = acquireAttackLayers();
                    const int writeIdx = 1 - voice.publishedIndex.load();
                    const auto& shares = voices[(size_t) v].buffers[(size_t) writeIdx]->attackBlendShare;
                    const float fadeMsCtl = Boreal::getParameterValueSafe<float> (
                        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::attack_fade_ms].ID, 30.0f);
                    const float windowMsCtl = Boreal::getParameterValueSafe<float> (
                        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::attack_window_ms].ID, 300.0f);

                    for (int s = 0; s < MorphEngine::numSlots; ++s)
                    {
                        if (! layers || ! layers->slots[(size_t) s].present)
                            continue;
                        if (shares[(size_t) s] < 1.0e-3f)
                            continue;

                        const auto& layer = layers->slots[(size_t) s];
                        if (layer.onsets.empty() || layer.durationSec <= 0.0)
                            continue;

                        const double sPrev = slotPrev[(size_t) s];
                        const double sNew = slotNew[(size_t) s];

                        if (sNew < sPrev && sNew < 0.05 && sPrev > 0.5)
                            voice.attackNext[(size_t) s] = 0;

                        if (! (sNew > sPrev))
                            continue;

                        const double tPrev = sPrev * layer.durationSec;
                        const double tNew = sNew * layer.durationSec;

                        while (voice.attackNext[(size_t) s] < layer.onsets.size()
                               && layer.onsets[voice.attackNext[(size_t) s]] <= tNew)
                        {
                            const double onset = layer.onsets[voice.attackNext[(size_t) s]];
                            ++voice.attackNext[(size_t) s];

                            if (onset <= tPrev)
                                continue;

                            const double rate = juce::jmax (1.0e-3,
                                std::abs ((double) ratePercent) / 100.0
                                * cursorParams[(size_t) s].rateMult);
                            const double overshootMat = tNew - onset;
                            const double compSmp = overshootMat * layer.sampleRate / rate;
                            const double pos = (onset - attackPreRollSec.load()) * layer.sampleRate
                                               - layer.posOffset - compSmp;

                            const int newHead = 1 - voice.attackHead[(size_t) s];
                            const int oldHead = voice.attackHead[(size_t) s];
                            const double chokeTail = std::min (std::max (1.0e-3, (double) fadeMsCtl / 1000.0), 0.0025);
                            if (voice.attackOn[(size_t) oldHead][(size_t) s])
                                voice.attackAge[(size_t) oldHead][(size_t) s]
                                    = std::max (voice.attackAge[(size_t) oldHead][(size_t) s],
                                                (double) windowMsCtl / 1000.0 - chokeTail);
                            voice.attackPos[(size_t) newHead][(size_t) s] = std::max (0.0, pos);
                            voice.attackAge[(size_t) newHead][(size_t) s] = 0.0;
                            voice.attackOn[(size_t) newHead][(size_t) s] = true;
                            voice.attackHead[(size_t) s] = newHead;
                            break;
                        }
                    }
            }

        const int writeIndex = 1 - voice.publishedIndex.load();
        auto& snapshot = *voice.buffers[(size_t) writeIndex];

        double slotTimes[MorphEngine::numSlots];
        for (int s = 0; s < MorphEngine::numSlots; ++s)
            slotTimes[(size_t) s] = voice.slotCursors[(size_t) s].phase;

        if (! morphEngine.computeMorph (x, y, voice.timeIndexPhase, snapshot.targets,
                                        xFreq, xAmp, xBw, slotTimes, formantShiftSt))
        {

            snapshot.targets.clear();
            voice.publishedIndex.store (writeIndex);
            continue;
        }

        voice.publishedIndex.store (writeIndex);
    }

    {
        int displayVoice = -1;
        uint32_t newest = 0;
        for (int v = 0; v < maxVoices; ++v)
        {
            auto& voice = voices[(size_t) v];
            if (voice.gateOn || voice.envelope.isActive())
            {
                if (displayVoice == -1 || voice.noteOnOrder >= newest)
                {
                    displayVoice = v;
                    newest = voice.noteOnOrder;
                }
            }
        }
        timeIndexDisplay.store (displayVoice >= 0 ? voices[(size_t) displayVoice].timeIndexPhase : -1.0);
        for (int s = 0; s < MorphEngine::numSlots; ++s)
            slotPhaseDisplay[(size_t) s].store (displayVoice >= 0
                ? (float) voices[(size_t) displayVoice].slotCursors[(size_t) s].phase : -1.0f);
    }
}

void BorealAudioProcessor::noteOn (int midiNoteNumber, float velocity)
{
    (void) velocity;
    const int numVoices = juce::jlimit (1, maxVoices, currentVoices.load());

    const bool mono = (numVoices == 1);
    if (mono && legatoEnabled.load())
    {
        auto& voice = voices[0];
        monoNoteStack.push_back (midiNoteNumber);

        if (voice.gateOn)
        {
            voice.noteNumber = midiNoteNumber;
            voice.noteOnOrder = ++noteOnCounter;
            return;
        }
    }

    Voice* chosen = nullptr;
    for (int v = 0; v < numVoices; ++v)
    {
        if (voices[(size_t) v].gateOn && voices[(size_t) v].noteNumber == midiNoteNumber)
        {
            chosen = &voices[(size_t) v];
            break;
        }
    }

    if (chosen == nullptr)
    {
        for (int v = 0; v < numVoices; ++v)
        {
            if (! voices[(size_t) v].gateOn && ! voices[(size_t) v].envelope.isActive())
            {
                chosen = &voices[(size_t) v];
                break;
            }
        }
    }

    if (chosen == nullptr)
    {
        uint32_t oldestOrder = std::numeric_limits<uint32_t>::max();
        for (int v = 0; v < numVoices; ++v)
        {
            if (voices[(size_t) v].noteOnOrder < oldestOrder)
            {
                oldestOrder = voices[(size_t) v].noteOnOrder;
                chosen = &voices[(size_t) v];
            }
        }
    }

    if (chosen == nullptr)
        return;

    chosen->envelope.noteOff();
    chosen->gateOn = true;
    chosen->noteNumber = midiNoteNumber;
    chosen->noteOnOrder = ++noteOnCounter;
    chosen->timeIndexPhase = 0.0;
    chosen->timeIndexRising = true;
    chosen->timeIndexSign = 1;
    chosen->envelope.noteOn();

    for (int s = 0; s < MorphEngine::numSlots; ++s)
    {
        const int base = (int) Boreal::Parameters::slot1_rate + s * 5;
        const double off = (double) Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[base + 1].ID, 0.0f) / 100.0;
        double w0 = (double) Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[base + 2].ID, 0.0f) / 100.0;
        double w1 = (double) Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[base + 3].ID, 100.0f) / 100.0;
        if (w1 <= w0 + 1.0e-6) { w0 = 0.0; w1 = 1.0; }
        const bool rev = Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[base + 4].ID, 0.0f) >= 0.5f;
        float rateAtNote = Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::time_scrub_rate].ID, 100.0f);
        if (! getTimeScrubEnabled())
            rateAtNote = 100.0f;
        const bool backward = (rateAtNote < 0.0f) != rev;
        chosen->slotCursors[(size_t) s].phase = backward
            ? w1 - juce::jlimit (0.0, w1 - w0, off) : juce::jlimit (w0, w1, off);
        chosen->slotCursors[(size_t) s].rising = (backward == (rateAtNote < 0.0f));
    }

    for (int h = 0; h < 2; ++h)
    {
        chosen->attackPos[(size_t) h].fill (0.0);
        chosen->attackAge[(size_t) h].fill (0.0);
        chosen->attackOn[(size_t) h].fill (false);
    }
    chosen->attackHead.fill (0);
    chosen->attackNext.fill (0);
}

void BorealAudioProcessor::noteOff (int midiNoteNumber)
{
    const int numVoices = juce::jlimit (1, maxVoices, currentVoices.load());

    if (numVoices == 1 && legatoEnabled.load())
    {
        auto it = std::find (monoNoteStack.begin(), monoNoteStack.end(), midiNoteNumber);
        if (it != monoNoteStack.end())
            monoNoteStack.erase (it);

        auto& voice = voices[0];
        if (! voice.gateOn)
            return;

        if (! monoNoteStack.empty())
        {
            voice.noteNumber = monoNoteStack.back();
            voice.noteOnOrder = ++noteOnCounter;
            return;
        }
    }

    for (int v = 0; v < numVoices; ++v)
    {
        auto& voice = voices[(size_t) v];
        if (voice.gateOn && voice.noteNumber == midiNoteNumber)
        {
            voice.gateOn = false;
            voice.envelope.noteOff();
        }
    }
}

void BorealAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (! midiMessages.isEmpty())
    {
        std::lock_guard<std::mutex> lock (midiMutex);
        for (const auto metadata : midiMessages)
        {
            const juce::MidiMessage msg = metadata.getMessage();

            if (msg.isNoteOn())
            {
                midiQueue.push_back ({ true,  msg.getNoteNumber(), msg.getFloatVelocity() });
            }
            else if (msg.isNoteOff())
            {
                midiQueue.push_back ({ false, msg.getNoteNumber(), 0.0f });
            }
            else if (msg.isPitchWheel())
            {
                const float bendRange = Boreal::getParameterValueSafe<float> (
                    &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::pitch_bend_range].ID, 2.0f);
                pitchBendSemitones = (double) bendRange * ((msg.getPitchWheelValue() - 8192) / 8192.0);
            }
        }
    }

    const double bendFactor = std::pow (2.0, pitchBendSemitones / 12.0);

    const juce::ADSR::Parameters adsrParams {
        Boreal::getParameterValueSafe<float> (&apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::asdr_attack].ID, 0.1f),
        Boreal::getParameterValueSafe<float> (&apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::asdr_decay].ID, 0.8f),
        Boreal::getParameterValueSafe<float> (&apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::asdr_sustain].ID, 0.8f),
        Boreal::getParameterValueSafe<float> (&apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::asdr_release].ID, 0.1f)
    };

    const int numSamples = buffer.getNumSamples();

    const float preserveAmt = juce::jlimit (0.0f, 1.0f,
        Boreal::getParameterValueSafe<float> (&apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::transient_preserve].ID, 0.0f) / 100.0f);
    const float attackFadeMs = Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::attack_fade_ms].ID, 30.0f);
    const float attackWindowMs = Boreal::getParameterValueSafe<float> (
        &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::attack_window_ms].ID, 300.0f);
    const double transposeMult = std::pow (2.0,
        (double) Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::transpose_st].ID, 0.0f) / 12.0
        + (double) Boreal::getParameterValueSafe<float> (
            &apvts, Boreal::PARAMETERS<float>[Boreal::Parameters::fine_tune_cents].ID, 0.0f) / 1200.0);
    auto attackLayers = acquireAttackLayers();

    for (int v = 0; v < maxVoices; ++v)
    {
        auto& voice = voices[(size_t) v];
        voice.envelope.setParameters (adsrParams);

        if (! voice.envelope.isActive())
            continue;

        auto snapshot = voice.buffers[(size_t) voice.publishedIndex.load()];
        const auto& targets = snapshot->targets;
        const int numTargets = (int) targets.size();

        if ((int) voice.scratchBuffer.size() < numSamples)
            voice.scratchBuffer.resize ((size_t) numSamples, 0.0);

        std::fill (voice.scratchBuffer.begin(), voice.scratchBuffer.begin() + numSamples, 0.0);

        const double refHz = referenceFreqHz.load();
        double pitchFactor = 1.0;
        if (voice.noteNumber >= 0)
        {
            constexpr double c4Hz = 261.6255653005986;
            const double noteHz = 440.0 * std::pow (2.0, ((double) voice.noteNumber - 69.0) / 12.0);
            const double rootHz = pitchLockEnabled.load() ? c4Hz : refHz;
            if (rootHz > 0.0)
                pitchFactor = (noteHz / rootHz) * bendFactor * transposeMult;
        }

        const float glideMs = glideTimeMs.load();
        if (glideMs > 0.0f && voice.noteNumber >= 0)
        {
            const double target = pitchFactor;
            if (voice.glideFactor < 0.0)
            {
                voice.glideFactor = target;
            }
            else
            {
                const double tau = (double) glideMs / 1000.0;
                const double dt = (double) numSamples / currentSampleRate;
                const double alpha = 1.0 - std::exp (-dt / tau);
                voice.glideFactor += (target - voice.glideFactor) * alpha;
            }
            pitchFactor = voice.glideFactor;
        }
        else
        {
            voice.glideFactor = -1.0;
        }

        constexpr float ampCullThreshold = 0.001f;

        if (voice.referencedStamp.size() < voice.oscillators.size())
            voice.referencedStamp.resize (voice.oscillators.size(), 0);

        bool remapNeeded = (int) voice.lastSnapshotIds.size() != numTargets;
        if (! remapNeeded)
        {
            for (int i = 0; i < numTargets; ++i)
            {
                if (targets[(size_t) i].sourceId != voice.lastSnapshotIds[(size_t) i])
                {
                    remapNeeded = true;
                    break;
                }
            }
        }

        if (remapNeeded)
        {
            voice.lastSnapshotIds.resize ((size_t) numTargets);
            voice.slotForTarget.resize ((size_t) numTargets);

            for (int i = 0; i < numTargets; ++i)
            {
                const int id = targets[(size_t) i].sourceId;
                voice.lastSnapshotIds[(size_t) i] = id;

                size_t slot;
                const auto it = voice.sourceIdToSlot.find (id);
                if (it != voice.sourceIdToSlot.end())
                {
                    slot = it->second;
                }
                else
                {

                    slot = voice.oscillators.size();
                    voice.oscillators.emplace_back();
                    voice.oscillatorSourceIds.push_back (id);
                    voice.sourceIdToSlot.emplace (id, slot);
                }

                voice.slotForTarget[(size_t) i] = (int) slot;
            }

            if (voice.referencedStamp.size() < voice.oscillators.size())
                voice.referencedStamp.resize (voice.oscillators.size(), 0);
        }

        if (++voice.currentStamp == 0)
        {
            std::fill (voice.referencedStamp.begin(), voice.referencedStamp.end(), 0u);
            voice.currentStamp = 1;
        }

        for (int i = 0; i < numTargets; ++i)
        {
            const auto& target = targets[(size_t) i];
            auto& oscillator = voice.oscillators[(size_t) voice.slotForTarget[(size_t) i]];
            voice.referencedStamp[(size_t) voice.slotForTarget[(size_t) i]] = voice.currentStamp;

            if (target.amplitude < ampCullThreshold && oscillator.amplitude() < ampCullThreshold)
                continue;

            Loris::Breakpoint bp;
            bp.setFrequency ((double) target.frequencyHz * pitchFactor);
            bp.setAmplitude ((double) target.amplitude);
            bp.setBandwidth ((double) target.bandwidth);
            bp.setPhase (0.0);

            oscillator.oscillate (voice.scratchBuffer.data(), voice.scratchBuffer.data() + numSamples, bp, currentSampleRate);
        }

        std::vector<int> slotsToRemove;
        const int poolSize = (int) voice.oscillators.size();
        for (int slot = 0; slot < poolSize; ++slot)
        {
            if (voice.referencedStamp[(size_t) slot] == voice.currentStamp)
                continue;

            auto& oscillator = voice.oscillators[(size_t) slot];
            if (oscillator.amplitude() <= 0.0)
            {
                slotsToRemove.push_back (slot);
                continue;
            }

            Loris::Breakpoint bp;
            bp.setFrequency (oscillator.radianFreq() * currentSampleRate / juce::MathConstants<double>::twoPi);
            bp.setAmplitude (0.0);
            bp.setBandwidth (oscillator.bandwidth());
            bp.setPhase (0.0);
            oscillator.oscillate (voice.scratchBuffer.data(), voice.scratchBuffer.data() + numSamples, bp, currentSampleRate);
        }

        if (! slotsToRemove.empty())
        {
            for (int k = (int) slotsToRemove.size() - 1; k >= 0; --k)
            {
                const int slot = slotsToRemove[(size_t) k];
                voice.oscillators.erase (voice.oscillators.begin() + slot);
                voice.oscillatorSourceIds.erase (voice.oscillatorSourceIds.begin() + slot);
                voice.referencedStamp.erase (voice.referencedStamp.begin() + slot);
            }

            voice.sourceIdToSlot.clear();
            for (size_t i = 0; i < voice.oscillatorSourceIds.size(); ++i)
                voice.sourceIdToSlot.emplace (voice.oscillatorSourceIds[i], i);

            voice.lastSnapshotIds.clear();
        }

        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        const bool layerActive = preserveAmt > 0.0f && attackLayers != nullptr;
        const double windowSec = (double) attackWindowMs / 1000.0;
        const double fadeDur = layerActive
            ? std::min ((double) attackFadeMs / 1000.0, 0.5 * windowSec)
            : 0.0;
        const double fadeStart = windowSec - fadeDur;
        constexpr double duckLeadSec = 0.001;

        double presentWeight = 0.0;
        if (layerActive)
            for (int s = 0; s < MorphEngine::numSlots; ++s)
                presentWeight += snapshot->attackBlendShare[(size_t) s];

        for (int i = 0; i < numSamples; ++i)
        {
            const float envGain = voice.envelope.getNextSample();
            double sample = 0.0;
            double partialGain = 1.0;

            if (layerActive && presentWeight > 1.0e-6)
            {
                double snippetSum = 0.0;
                double activeWeight = 0.0;
                double duckShape = 0.0;

                for (int s = 0; s < MorphEngine::numSlots; ++s)
                {
                    const auto& layer = attackLayers->slots[(size_t) s];
                    if (! layer.present)
                        continue;

                    const auto& data = layer.samples;
                    const size_t len = data.size();
                    const double readRate = pitchFactor * layer.sampleRate;

                    for (int h = 0; h < 2; ++h)
                    {
                        if (! voice.attackOn[(size_t) h][(size_t) s])
                            continue;

                        double pos = voice.attackPos[(size_t) h][(size_t) s];
                        if (pos >= (double) len)
                        {
                            voice.attackOn[(size_t) h][(size_t) s] = false;
                            continue;
                        }

                        const double age = voice.attackAge[(size_t) h][(size_t) s];

                        const double remainingSec = ((double) len - pos) / std::max (1e-9, readRate);
                        const double tailProgress = (age - fadeStart) / std::max (1.0e-9, fadeDur);
                        const double endGuard = 1.0 - remainingSec / std::max (1.0e-9, fadeDur);
                        const double progress = juce::jlimit (0.0, 1.0,
                            std::max (tailProgress, endGuard));
                        const double theta = 0.5 * juce::MathConstants<double>::pi * progress;

                        activeWeight += snapshot->attackBlendShare[(size_t) s];
                        duckShape = std::max (duckShape,
                            (1.0 - std::sin (theta)) * std::min (1.0, age / duckLeadSec));

                        const size_t idx = (size_t) pos;
                        const double fracs = pos - (double) idx;
                        const double vraw = (idx + 1 < len)
                            ? ((double) data[idx] * (1.0 - fracs) + (double) data[idx + 1] * fracs)
                            : (double) data[idx];
                        snippetSum += vraw * snapshot->attackBlendShare[(size_t) s]
                                      * (double) layer.loudnessGain * (double) preserveAmt
                                      * std::cos (theta);

                        pos += readRate / currentSampleRate;
                        voice.attackAge[(size_t) h][(size_t) s] = age + 1.0 / currentSampleRate;
                        if (pos >= (double) len || age + 1.0 / currentSampleRate >= windowSec)
                            voice.attackOn[(size_t) h][(size_t) s] = false;
                        else
                            voice.attackPos[(size_t) h][(size_t) s] = pos;
                    }
                }

                const double duckDepth = (activeWeight / presentWeight) * duckShape;
                if (duckDepth > 0.0)
                    partialGain = 1.0 - (double) preserveAmt * std::min (1.0, duckDepth);
                sample += snippetSum * (double) envGain;
            }

            const float out = (float) ((double) voice.scratchBuffer[(size_t) i] * envGain * partialGain + sample);
            left[i] += out;
            if (right != nullptr)
                right[i] += out;
        }
    }

    int activeVoices = 0;
    for (auto& voice : voices)
        if (voice.envelope.isActive())
            ++activeVoices;

    const float masterTrim = activeVoices > 0 ? 1.0f / std::sqrt ((float) activeVoices) : 1.0f;
    buffer.applyGain (masterTrim);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        juce::dsp::FastMathApproximations::tanh (buffer.getWritePointer (ch), (size_t) numSamples);

    if (previewActive.load())
    {
        float* const* chans = buffer.getArrayOfWritePointers();
        renderPreview (chans, buffer.getNumChannels(), numSamples);
    }

    buffer.applyGain (juce::Decibels::decibelsToGain<float> (
        gainParam->get()));

    float blockPeak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        blockPeak = juce::jmax (blockPeak, buffer.getMagnitude (ch, 0, numSamples));
    outputPeak.store (blockPeak);
}

juce::AudioProcessorEditor* BorealAudioProcessor::createEditor()
{
    return new BorealAudioProcessorEditor (*this);
}

void BorealAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::unique_ptr<juce::XmlElement> xml (new juce::XmlElement ("BOREAL_STATE"));

    xml->setAttribute ("voices", (int) voicesValue.getValue());
    xml->setAttribute ("timeScrub", getTimeScrubEnabled());
    xml->setAttribute ("loop", getLoopEnabled());
    xml->setAttribute ("pitchLock", getPitchLockEnabled());
    xml->setAttribute ("forwardOnly", getForwardOnlyEnabled());
    xml->setAttribute ("legato", getLegatoEnabled());

    xml->setAttribute ("analyzerResolutionHz", getAnalyzerResolutionHz());
    xml->setAttribute ("analyzerAmpFloorDb", getAnalyzerAmpFloorDb());
    xml->setAttribute ("analyzerCleanPartials", getAnalyzerCleanPartials());
    xml->setAttribute ("analyzerOnsetSensitivity", getAnalyzerOnsetSensitivity());
    xml->setAttribute ("analyzerPreRollMs", getAnalyzerPreRollMs());

    xml->setAttribute ("tooltipDelayMs", getTooltipDelayMs());
    xml->setAttribute ("tooltipsEnabled", getTooltipsEnabled());
    xml->setAttribute ("confirmDestructive", getConfirmDestructive());

    xml->setAttribute ("uiMode", getUiMode());
    xml->setAttribute ("analyzerWindowWidthHz", getAnalyzerWindowWidthHz());
    xml->setAttribute ("analyzerLoCutHz", getAnalyzerLoCutHz());
    xml->setAttribute ("analyzerHiCutHz", getAnalyzerHiCutHz());
    xml->setAttribute ("analyzerFreqDriftHz", getAnalyzerFreqDriftHz());
    xml->setAttribute ("analyzerNoiseWidthHz", getAnalyzerNoiseWidthHz());
    xml->setAttribute ("analyzerFundamentalHz", getAnalyzerFundamentalHz());
    xml->setAttribute ("previewSrcVolDb", (double) previewSrcVolDbValue.getValue());
    xml->setAttribute ("previewRsynVolDb", (double) previewRsynVolDbValue.getValue());

    for (int i = 0; i < MorphEngine::numSlots; i++)
    {
        xml->setAttribute ("slot" + juce::String (i + 1), morphEngine.getSlotFilePath (i));
        xml->setAttribute ("slotSourceAudio" + juce::String (i + 1), slotSourceAudio[(size_t) i]);
    }

    if (auto state = apvts.copyState(); true)
    {
        std::unique_ptr<juce::XmlElement> apvtsXml (state.createXml());
        xml->addChildElement (apvtsXml.release());
    }

    copyXmlToBinary (*xml, destData);
}

void BorealAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;

    if (xml->hasAttribute ("voices"))
        setCurrentVoices (xml->getIntAttribute ("voices", 1));
    if (xml->hasAttribute ("timeScrub"))
        setTimeScrubEnabled (xml->getBoolAttribute ("timeScrub", true));
    if (xml->hasAttribute ("loop"))
        setLoopEnabled (xml->getBoolAttribute ("loop", true));
    if (xml->hasAttribute ("pitchLock"))
        setPitchLockEnabled (xml->getBoolAttribute ("pitchLock", false));
    if (xml->hasAttribute ("forwardOnly"))
        setForwardOnlyEnabled (xml->getBoolAttribute ("forwardOnly", false));
    if (xml->hasAttribute ("legato"))
        setLegatoEnabled (xml->getBoolAttribute ("legato", false));

    if (xml->hasAttribute ("analyzerResolutionHz"))
        analyzerResolutionValue = (double) xml->getDoubleAttribute ("analyzerResolutionHz", 40.0);
    if (xml->hasAttribute ("analyzerAmpFloorDb"))
        analyzerAmpFloorValue = (double) xml->getDoubleAttribute ("analyzerAmpFloorDb", -60.0);
    if (xml->hasAttribute ("analyzerCleanPartials"))
        analyzerCleanPartialsValue = xml->getBoolAttribute ("analyzerCleanPartials", false);
    else if (xml->hasAttribute ("analyzerPipeline"))
        analyzerCleanPartialsValue = xml->getBoolAttribute ("analyzerPipeline", false);
    if (xml->hasAttribute ("analyzerOnsetSensitivity"))
        analyzerOnsetSensitivityValue = (double) xml->getDoubleAttribute (
            "analyzerOnsetSensitivity",
            boreal::analyzer::Settings::kOnsetSensDefault);
    else if (xml->hasAttribute ("analyzerOnsetMult"))
        analyzerOnsetSensitivityValue = boreal::analyzer::Settings::onsetMultToSensitivity (
            (double) xml->getDoubleAttribute ("analyzerOnsetMult", 1.6));
    if (xml->hasAttribute ("analyzerPreRollMs"))
    {
        const double ms = xml->getDoubleAttribute ("analyzerPreRollMs", 1.0);
        analyzerPreRollMsValue = ms;
        attackPreRollSec.store (ms / 1000.0);
    }

    if (xml->hasAttribute ("tooltipDelayMs"))
        tooltipDelayMsValue = xml->getIntAttribute ("tooltipDelayMs", 700);
    if (xml->hasAttribute ("tooltipsEnabled"))
        tooltipsEnabledValue = xml->getBoolAttribute ("tooltipsEnabled", true);
    if (xml->hasAttribute ("confirmDestructive"))
        confirmDestructiveValue = xml->getBoolAttribute ("confirmDestructive", true);

    if (xml->hasAttribute ("uiMode"))
        uiModeValue = xml->getStringAttribute ("uiMode", "performance");
    if (xml->hasAttribute ("analyzerWindowWidthHz"))
        analyzerWindowWidthHzValue = xml->getDoubleAttribute ("analyzerWindowWidthHz", 80.0);
    if (xml->hasAttribute ("analyzerLoCutHz"))
        analyzerLoCutHzValue = xml->getDoubleAttribute ("analyzerLoCutHz", 20.0);
    if (xml->hasAttribute ("analyzerHiCutHz"))
        analyzerHiCutHzValue = xml->getDoubleAttribute ("analyzerHiCutHz", 20000.0);
    if (xml->hasAttribute ("analyzerFreqDriftHz"))
        analyzerFreqDriftHzValue = xml->getDoubleAttribute ("analyzerFreqDriftHz", 40.0);
    if (xml->hasAttribute ("analyzerNoiseWidthHz"))
        analyzerNoiseWidthHzValue = xml->getDoubleAttribute ("analyzerNoiseWidthHz", 500.0);
    if (xml->hasAttribute ("analyzerFundamentalHz"))
        analyzerFundamentalHzValue = xml->getDoubleAttribute ("analyzerFundamentalHz", 0.0);
    if (xml->hasAttribute ("previewSrcVolDb"))
    {

        const double v = xml->getDoubleAttribute ("previewSrcVolDb", 0.0);
        previewSrcVolDbValue = (v < -40.0) ? 0.0 : v;
    }
    if (xml->hasAttribute ("previewRsynVolDb"))
    {
        const double v = xml->getDoubleAttribute ("previewRsynVolDb", 0.0);
        previewRsynVolDbValue = (v < -40.0) ? 0.0 : v;
    }

    for (int i = 0; i < MorphEngine::numSlots; i++)
    {
        const juce::String path = xml->getStringAttribute ("slot" + juce::String (i + 1));
        if (path.isNotEmpty() && juce::File (path).existsAsFile())
            morphEngine.loadSlot (i, juce::File (path));

        slotSourceAudio[(size_t) i]
            = xml->getStringAttribute ("slotSourceAudio" + juce::String (i + 1));
    }
    refreshAttackLayers();

    if (auto* child = xml->getChildByName ("PARAMS"))
        apvts.replaceState (juce::ValueTree::fromXml (*child));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BorealAudioProcessor();
}
