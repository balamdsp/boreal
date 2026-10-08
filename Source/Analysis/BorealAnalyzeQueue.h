#pragma once

#include <JuceHeader.h>

#include "BorealAnalyzer.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>

class BorealAnalyzeQueue : private juce::Thread
{
public:

    using LoadHandoff = std::function<void (int slotIndex, const juce::File& sdifFile)>;

    using FailureNotify = std::function<void (int slotIndex, const juce::String& errorMessage)>;

    using WriteDoneNotify = std::function<void (int slotIndex, bool ok, const juce::String& errorMessage)>;

    using ResultPtr = std::shared_ptr<const boreal::analyzer::Result>;
    using ResultNotify = std::function<void (int slotIndex, bool ok, ResultPtr result, const juce::String& errorMessage)>;

    enum class Kind
    {
        Analyze,
        Synthesize
    };

    struct Request
    {

        int slotIndex = -1;
        Kind kind = Kind::Analyze;
        juce::File sourceAudio;
        juce::File outputBase;

        boreal::analyzer::Settings settings;
        bool autoLoad = true;

        bool deliverResult = false;

        boreal::analyzer::PartialFrameData synthInput;
        double synthSampleRate = 44100.0;
    };

    BorealAnalyzeQueue()
        : juce::Thread ("Boreal Analyzer")
    {
    }

    ~BorealAnalyzeQueue() override
    {

        {
            const std::lock_guard<std::mutex> lock (queueMutex);
            pending.clear();
        }
        signalThreadShouldExit();
        wakeup.signal();
        stopThread (-1);
    }

    void configure (LoadHandoff onSdifReady, FailureNotify onFailure, WriteDoneNotify onWriteDone, ResultNotify onResult)
    {
        handoff = std::move (onSdifReady);
        failureNotify = std::move (onFailure);
        writeDoneNotify = std::move (onWriteDone);
        resultNotify = std::move (onResult);

        if (! isThreadRunning())
            startThread (juce::Thread::Priority::normal);
    }

    void enqueue (const Request& request)
    {
        {
            const std::lock_guard<std::mutex> lock (queueMutex);

            if (request.slotIndex >= 0)
                pending.erase (std::remove_if (pending.begin(), pending.end(),
                                               [&] (const Request& r)
                                               { return r.slotIndex == request.slotIndex; }),
                               pending.end());
            pending.push_back (request);
        }
        wakeup.signal();
    }

    bool isBusy() const noexcept { return jobActive.load(); }
    int busySlot() const noexcept { return activeSlot.load(); }

    int currentStageIndex() const noexcept { return jobActive.load() ? stage.load() : -1; }
    float overallProgress() const noexcept { return isBusy() ? progress01.load() : 0.0f; }
    int queuedCount() const noexcept
    {
        const std::lock_guard<std::mutex> lock (queueMutex);
        return (int) pending.size();
    }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            Request job;
            bool haveJob = false;
            {
                const std::lock_guard<std::mutex> lock (queueMutex);
                if (! pending.empty())
                {
                    job = pending.front();
                    pending.pop_front();
                    haveJob = true;
                }
            }

            if (! haveJob)
            {
                wakeup.wait();
                continue;
            }

            runJob (job);
        }
    }

    void runJob (const Request& job)
    {
        activeSlot.store (job.slotIndex);
        jobActive.store (true);
        stage.store ((int) (job.kind == Kind::Synthesize
                                ? boreal::analyzer::Stage::Writing
                                : boreal::analyzer::Stage::Reading));
        progress01.store (0.0f);

        auto progress = [this] (boreal::analyzer::Stage s, float overall)
        {
            if (threadShouldExit())
                return false;
            stage.store ((int) s);
            progress01.store (overall);
            return true;
        };

        auto result = std::make_shared<boreal::analyzer::Result>();
        std::string err;
        bool ok = false;
        juce::File writtenSdif;

        if (job.kind == Kind::Synthesize)
        {
            ok = boreal::analyzer::synthesizePartials (job.synthInput, job.synthSampleRate,
                                                       result->renderedSamples, &err);
            result->renderedSampleRate = ok ? job.synthSampleRate : 0.0;
            result->partials = job.synthInput;
            progress01.store (1.0f);
        }
        else
        {
            ok = boreal::analyzer::analyzeToFiles (
                job.sourceAudio.getFullPathName().toStdString(),
                job.outputBase.getFullPathName().toStdString(),
                job.settings, progress, result.get(), &err);
            writtenSdif = juce::File (result->sdifPath);
            if (! ok)
                *result = boreal::analyzer::Result();
        }

        activeSlot.store (-1);
        jobActive.store (false);
        stage.store (-1);
        progress01.store (0.0f);

        if (threadShouldExit())
            return;

        if (job.deliverResult)
        {
            auto onResultCopy = resultNotify;
            ResultPtr delivered = ok ? result : nullptr;
            const bool success = ok;
            const juce::String message = juce::String (err);
            juce::MessageManager::callAsync ([onResultCopy, slot = job.slotIndex, success,
                                              delivered, message]
            {
                if (onResultCopy)
                    onResultCopy (slot, success, success ? delivered : nullptr, message);
            });
            return;
        }

        if (ok && ! result->sdifPath.empty() && job.autoLoad)
        {

            if (handoff)
                handoff (job.slotIndex, writtenSdif);
            return;
        }

        auto onFailureCopy = failureNotify;
        auto onWriteDoneCopy = writeDoneNotify;
        const bool success = ok && ! result->sdifPath.empty();
        juce::MessageManager::callAsync ([onFailureCopy, onWriteDoneCopy, slot = job.slotIndex,
                                          success, message = juce::String (err)]
        {
            if (success)
            {
                if (onWriteDoneCopy)
                    onWriteDoneCopy (slot, true, {});
            }
            else
            {
                const juce::String text = message.isEmpty()
                    ? "Analysis failed"
                    : message;
                if (onFailureCopy)
                    onFailureCopy (slot, text);
                else if (onWriteDoneCopy)
                    onWriteDoneCopy (slot, false, text);
            }
        });
    }

    LoadHandoff handoff;
    FailureNotify failureNotify;
    WriteDoneNotify writeDoneNotify;
    ResultNotify resultNotify;

    mutable std::mutex queueMutex;
    std::deque<Request> pending;
    juce::WaitableEvent wakeup;

    std::atomic<int> activeSlot { -1 };
    std::atomic<bool> jobActive { false };
    std::atomic<int> stage { -1 };
    std::atomic<float> progress01 { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealAnalyzeQueue)
};
