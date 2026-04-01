#include "PluginEditor.h"

JacquardEditor::JacquardEditor(JacquardProcessor& proc)
    : AudioProcessorEditor(proc)
    , proc_(proc)
    , timelineBar_(proc.getTimeline(), proc.getOptions(), proc.getSessionManager())
    , heatmap_(proc.getTimeline(), proc.getOptions(), proc.getSessionManager())
    , optionsPanel_(proc.getOptions())
{
    const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
    int height = 640 + (standalone ? kTransportH : 0);
    setSize(1000, height);
    setResizable(true, true);
    setResizeLimits(600, 400, 2400, 1800);

    addAndMakeVisible(timelineBar_);
    addAndMakeVisible(heatmap_);
    addAndMakeVisible(optionsPanel_);

    transportPanel_.setVisible(standalone);
    addChildComponent(transportPanel_); // always added, conditionally shown

    // ── Timeline → heatmap chord selection + preview ──────────────────────────
    timelineBar_.onChordSelected = [this](const juce::Uuid& id)
    {
        heatmap_.setSelectedChord(id);
        proc_.onTimelineChanged();

        if (proc_.getOptions().preview)
        {
            auto chord = proc_.getTimeline().chordById(id);
            if (chord.has_value())
                proc_.triggerPreview(*chord);
        }
    };

    // ── Options ───────────────────────────────────────────────────────────────
    optionsPanel_.onOptionsChanged = [this]()
    {
        proc_.applyOptions();
        proc_.onTimelineChanged();
        double s = proc_.getOptions().viewStartPPQ;
        double e = proc_.getOptions().viewEndPPQ;
        timelineBar_.setViewRange(s, e);
        heatmap_.setViewRange(s, e);
    };

    // ── Standalone transport ──────────────────────────────────────────────────
    if (standalone)
    {
        transportPanel_.onPlayPauseChanged = [this](bool playing)
        {
            proc_.setStandalonePlaying(playing);
        };
        transportPanel_.onStop = [this]()
        {
            proc_.setStandalonePlaying(false);
            proc_.setStandalonePosition(0.0);
        };
        transportPanel_.onBpmChanged = [this](double bpm)
        {
            proc_.setStandaloneBpm(bpm);
        };
    }

    proc_.onTimelineChanged();
    startTimerHz(30);
}

JacquardEditor::~JacquardEditor()
{
    stopTimer();
}

void JacquardEditor::onHeatmapRebuilt(std::shared_ptr<std::vector<ChordHeatmap>> data)
{
    heatmap_.onHeatmapRebuilt(std::move(data));
}

void JacquardEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(12, 12, 16));
}

void JacquardEditor::resized()
{
    auto area = getLocalBounds();
    timelineBar_.setBounds(area.removeFromTop(kTimelineH));
    optionsPanel_.setBounds(area.removeFromBottom(kOptionsH));

    if (juce::JUCEApplicationBase::isStandaloneApp())
        transportPanel_.setBounds(area.removeFromBottom(kTransportH));

    heatmap_.setBounds(area);
}

void JacquardEditor::timerCallback()
{
    double ppq = proc_.getPlayheadPPQ();
    double bpm = proc_.getStandaloneBpm();
    timelineBar_.setPlayheadPPQ(ppq);
    heatmap_.setPlayheadPPQ(ppq);
    transportPanel_.setPositionPPQ(ppq, bpm);

    double s = proc_.getOptions().viewStartPPQ;
    double e = proc_.getOptions().viewEndPPQ;
    timelineBar_.setViewRange(s, e);
    heatmap_.setViewRange(s, e);
}
