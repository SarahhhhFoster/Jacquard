#pragma once
#include "PluginProcessor.h"
#include "ui/HeatmapComponent.h"
#include "ui/TimelineBar.h"
#include "ui/OptionsPanel.h"
#include "ui/TransportPanel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

class JacquardEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit JacquardEditor(JacquardProcessor& processor);
    ~JacquardEditor() override;

    void onHeatmapRebuilt(std::shared_ptr<std::vector<ChordHeatmap>> data);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    JacquardProcessor& proc_;

    TimelineBar      timelineBar_;
    HeatmapComponent heatmap_;
    OptionsPanel     optionsPanel_;
    TransportPanel   transportPanel_;   // visible standalone only

    static constexpr int kTimelineH  = 60;
    static constexpr int kOptionsH   = 90;
    static constexpr int kTransportH = 34;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JacquardEditor)
};
