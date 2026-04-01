#include "PluginProcessor.h"
#include "PluginEditor.h"

// ── Constructor / Destructor ──────────────────────────────────────────────────

JacquardProcessor::JacquardProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    timeline_.onChanged = [this]() { onTimelineChanged(); };

    heatmap_.onRebuilt = [this](std::shared_ptr<std::vector<ChordHeatmap>> data)
    {
        if (auto* ed = getActiveEditor())
            if (auto* jed = dynamic_cast<JacquardEditor*>(ed))
                jed->onHeatmapRebuilt(std::move(data));
    };

    session_.onSessionChanged = [this]()
    {
        onTimelineChanged();
        if (auto* ed = getActiveEditor())
            ed->repaint();
    };

    session_.onToneStructural = [this]()
    {
        heatmap_.invalidate(timeline_, options_);
    };

    session_.start();
    startTimerHz(30);
}

JacquardProcessor::~JacquardProcessor()
{
    stopTimer();
    session_.stop();
}

// ── AudioProcessor ────────────────────────────────────────────────────────────

void JacquardProcessor::prepareToPlay(double sampleRate, int)
{
    sampleRate_ = sampleRate;
    blockFrame_ = 0;
    wasPlaying_ = false;
    sineBank_.setSampleRate(sampleRate);
}

void JacquardProcessor::releaseResources()
{
    juce::MidiBuffer dummy;
    mpe_.allNotesOff(dummy, 0);
    sineBank_.clear();
}

bool JacquardProcessor::isBusesLayoutSupported(const AudioProcessor::BusesLayout&) const
{
    return true;
}

void JacquardProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer&        midiOut)
{
    buffer.clear();

    const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
    bool isPlaying = false;
    double ppq     = 0.0;

    if (standalone)
    {
        // Drive from internal transport
        isPlaying = standaloneIsPlaying_.load(std::memory_order_relaxed);
        ppq       = standalonePPQ_.load(std::memory_order_relaxed);

        if (isPlaying)
        {
            double bpm           = standaloneBpm_.load(std::memory_order_relaxed);
            double beatsPerSample = bpm / (60.0 * sampleRate_);
            double newPPQ        = ppq + buffer.getNumSamples() * beatsPerSample;
            standalonePPQ_.store(newPPQ, std::memory_order_relaxed);
            ppq = newPPQ;
        }
    }
    else
    {
        // Drive from host playhead
        if (auto* head = getPlayHead())
        {
            if (auto posOpt = head->getPosition())
            {
                isPlaying = posOpt->getIsPlaying();
                ppq       = posOpt->getPpqPosition().orFallback(0.0);
            }
        }
    }

    playheadPPQ_.store(ppq, std::memory_order_relaxed);

    if (!isPlaying)
    {
        if (wasPlaying_)
        {
            mpe_.allNotesOff(midiOut, 0);
            wasPlaying_ = false;
        }
        // In standalone, still process the sine bank so preview tones play
        // even while transport is stopped
        if (standalone)
            sineBank_.process(buffer, 0, buffer.getNumSamples());
        ++blockFrame_;
        return;
    }

    wasPlaying_ = true;
    mpe_.process(midiOut, 0, blockFrame_);

    // Sine wave output in standalone mode
    if (standalone)
        sineBank_.process(buffer, 0, buffer.getNumSamples());

    ++blockFrame_;
}

// ── State ─────────────────────────────────────────────────────────────────────

void JacquardProcessor::getStateInformation(juce::MemoryBlock& data)
{
    juce::ValueTree root("JacquardState");
    root.addChild(timeline_.toValueTree(), -1, nullptr);
    root.addChild(options_.toValueTree(),  -1, nullptr);

    juce::MemoryOutputStream stream(data, false);
    root.writeToStream(stream);
}

void JacquardProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, (size_t)sizeInBytes, false);
    auto root = juce::ValueTree::readFromStream(stream);
    if (!root.isValid()) return;

    auto tlTree = root.getChildWithName("ChordTimeline");
    if (tlTree.isValid())
        timeline_.fromValueTree(tlTree);

    auto opTree = root.getChildWithName("Options");
    if (opTree.isValid())
        options_ = Options::fromValueTree(opTree);

    applyOptions();
    onTimelineChanged();
    heatmap_.invalidate(timeline_, options_);  // rebuild on state load
}

// ── Editor ────────────────────────────────────────────────────────────────────

juce::AudioProcessorEditor* JacquardProcessor::createEditor()
{
    return new JacquardEditor(*this);
}

// ── Preview ───────────────────────────────────────────────────────────────────

void JacquardProcessor::triggerPreview(const Chord& chord)
{
    if (!options_.preview) return;

    std::vector<Tone> tones;
    for (const auto& t : chord.tones)
        if (t.enabled) tones.push_back(t);

    mpe_.setTargetTones(tones);
    sineBank_.setTones(tones);

    previewActive_  = true;
    previewStartMs_ = juce::Time::getMillisecondCounter();
}

void JacquardProcessor::stopPreview()
{
    previewActive_ = false;
    mpe_.setTargetTones({});
    sineBank_.clear();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void JacquardProcessor::applyOptions()
{
    if (options_.output && !options_.outputDeviceId.isEmpty())
    {
        for (const auto& dev : juce::MidiOutput::getAvailableDevices())
        {
            if (dev.identifier == options_.outputDeviceId)
            {
                mpe_.setOutputDevice(juce::MidiOutput::openDevice(dev.identifier));
                break;
            }
        }
    }
    else
    {
        mpe_.setOutputDevice(nullptr);
    }
}

void JacquardProcessor::onTimelineChanged()
{
    // Heatmap is intentionally NOT rebuilt here — only tone structural changes
    // (add/remove/toggle) trigger a rebuild via session_.onToneStructural.
    // This prevents the heatmap from flickering while the user moves chords.
}

std::vector<Tone> JacquardProcessor::tonesAtPPQ(double ppq) const
{
    auto ctx = timeline_.contextAt(ppq);
    std::vector<Tone> result;
    if (ctx.current)
        for (const auto& t : ctx.current->tones)
            if (t.enabled && t.start <= ppq && t.stop > ppq)
                result.push_back(t);
    return result;
}

void JacquardProcessor::timerCallback()
{
    // Handle preview expiry
    if (previewActive_)
    {
        auto elapsed = juce::Time::getMillisecondCounter() - previewStartMs_;
        if (elapsed >= (uint32_t)kPreviewDurationMs)
            stopPreview();
        return; // Don't overwrite target tones while preview is active
    }

    // Update MPE / sine targets from playhead
    double ppq = playheadPPQ_.load(std::memory_order_relaxed);
    auto   tones = tonesAtPPQ(ppq);
    mpe_.setTargetTones(tones);

    if (juce::JUCEApplicationBase::isStandaloneApp())
        sineBank_.setTones(tones);
}

// ── Plugin entry point ────────────────────────────────────────────────────────

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JacquardProcessor();
}
