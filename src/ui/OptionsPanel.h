#pragma once
#include "../model/Options.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>

// OptionsPanel is the control strip at the bottom of the editor.
// It exposes loop, output, followServer, preview toggles, plus the
// snap controls, contrast/brightness sliders, and MIDI device selector.

class OptionsPanel : public juce::Component,
                     private juce::Button::Listener,
                     private juce::Slider::Listener,
                     private juce::ComboBox::Listener
{
public:
    explicit OptionsPanel(Options& options);
    ~OptionsPanel() override;

    // Repopulate the device list (call whenever devices change or on init)
    void refreshDeviceList();

    // Called when any option changes
    std::function<void()> onOptionsChanged;

    // Called when the user clicks the "Clear All" button
    std::function<void()> onClearAll;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;
    void comboBoxChanged(juce::ComboBox* c) override;

    Options& options_;

    juce::ToggleButton loopBtn_    { "Loop"   };
    juce::ToggleButton outputBtn_  { "Output" };
    juce::ToggleButton followBtn_  { "Follow" };
    juce::ToggleButton previewBtn_ { "Preview"};
    juce::TextButton   clearAllBtn_{ "Clear All" };

    juce::Label contrastLabel_, brightnessLabel_, snapLabel_, deviceLabel_, stratLabel_,
                neighbourLabel_;
    juce::Slider contrastSlider_, brightnessSlider_, snapSlider_, neighbourSlider_;

    juce::ComboBox snapStrategyBox_;
    juce::ComboBox deviceBox_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OptionsPanel)
};
