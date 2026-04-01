#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// TransportPanel is shown only when running as a standalone app.
// It provides a simple internal transport: play/pause, stop (rewind), BPM.

class TransportPanel : public juce::Component,
                       private juce::Button::Listener,
                       private juce::Label::Listener
{
public:
    TransportPanel();
    ~TransportPanel() override;

    // Set display state (called from editor timer at 30 Hz)
    void setIsPlaying(bool playing);
    void setPositionPPQ(double ppq, double bpm);

    // Callbacks → editor → processor
    std::function<void(bool)>   onPlayPauseChanged; // true = play
    std::function<void()>       onStop;             // rewind to 0
    std::function<void(double)> onBpmChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked(juce::Button* b) override;
    void labelTextChanged(juce::Label* lbl) override;

    juce::TextButton playPauseBtn_{ "Play" };
    juce::TextButton stopBtn_     { "Stop" };
    juce::Label      bpmLabel_    { {}, "BPM" };
    juce::Label      bpmValue_;
    juce::Label      posLabel_;

    bool isPlaying_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportPanel)
};
