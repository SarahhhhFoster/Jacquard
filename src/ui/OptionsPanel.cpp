#include "OptionsPanel.h"

OptionsPanel::OptionsPanel(Options& options) : options_(options)
{
    auto addLabel = [this](juce::Label& lbl, const juce::String& text)
    {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(10.0f));
        lbl.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
        addAndMakeVisible(lbl);
    };

    // Toggle buttons
    for (auto* btn : { &loopBtn_, &outputBtn_, &followBtn_, &previewBtn_ })
    {
        btn->addListener(this);
        btn->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        addAndMakeVisible(btn);
    }

    // Clear All button
    clearAllBtn_.addListener(this);
    clearAllBtn_.setColour(juce::TextButton::buttonColourId,   juce::Colour(140, 40, 40));
    clearAllBtn_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(180, 60, 60));
    clearAllBtn_.setColour(juce::TextButton::textColourOffId,  juce::Colours::white);
    addAndMakeVisible(clearAllBtn_);
    loopBtn_.setToggleState(options_.loop,         juce::dontSendNotification);
    outputBtn_.setToggleState(options_.output,     juce::dontSendNotification);
    followBtn_.setToggleState(options_.followServer,juce::dontSendNotification);
    previewBtn_.setToggleState(options_.preview,   juce::dontSendNotification);

    // Contrast slider
    addLabel(contrastLabel_, "Contrast");
    contrastSlider_.setRange(0.1, 5.0, 0.01);
    contrastSlider_.setValue(options_.heatMapContrast, juce::dontSendNotification);
    contrastSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    contrastSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 16);
    contrastSlider_.addListener(this);
    addAndMakeVisible(contrastSlider_);

    // Brightness slider
    addLabel(brightnessLabel_, "Brightness");
    brightnessSlider_.setRange(-1.0, 1.0, 0.01);
    brightnessSlider_.setValue(options_.heatMapBrightness, juce::dontSendNotification);
    brightnessSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    brightnessSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 16);
    brightnessSlider_.addListener(this);
    addAndMakeVisible(brightnessSlider_);

    // Snap range slider (cents)
    addLabel(snapLabel_, "Snap (¢)");
    snapSlider_.setRange(0.0, 200.0, 1.0);
    snapSlider_.setValue(options_.snapRange, juce::dontSendNotification);
    snapSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    snapSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 16);
    snapSlider_.addListener(this);
    addAndMakeVisible(snapSlider_);

    // Snap strategy
    addLabel(stratLabel_, "Strategy");
    snapStrategyBox_.addItem("Slope", 1);
    snapStrategyBox_.addItem("Value", 2);
    snapStrategyBox_.setSelectedId(options_.snapStrategy == SnapStrategy::Slope ? 1 : 2,
                                   juce::dontSendNotification);
    snapStrategyBox_.addListener(this);
    addAndMakeVisible(snapStrategyBox_);

    // Device picker
    addLabel(deviceLabel_, "MIDI Out");
    deviceBox_.addListener(this);
    addAndMakeVisible(deviceBox_);

    refreshDeviceList();
}

OptionsPanel::~OptionsPanel()
{
    clearAllBtn_.removeListener(this);
    for (auto* btn : { &loopBtn_, &outputBtn_, &followBtn_, &previewBtn_ })
        btn->removeListener(this);
    contrastSlider_.removeListener(this);
    brightnessSlider_.removeListener(this);
    snapSlider_.removeListener(this);
    snapStrategyBox_.removeListener(this);
    deviceBox_.removeListener(this);
}

void OptionsPanel::refreshDeviceList()
{
    deviceBox_.clear(juce::dontSendNotification);
    deviceBox_.addItem("(none)", 1);
    int idx = 2;
    for (const auto& dev : juce::MidiOutput::getAvailableDevices())
    {
        deviceBox_.addItem(dev.name, idx++);
        if (dev.identifier == options_.outputDeviceId)
            deviceBox_.setSelectedId(idx - 1, juce::dontSendNotification);
    }
}

void OptionsPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(18, 18, 22));
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());
}

void OptionsPanel::resized()
{
    auto area = getLocalBounds().reduced(4, 4);
    int  rowH = area.getHeight() / 2;
    int  col  = 0;

    // Row 1: toggles + contrast + brightness
    auto row1 = area.removeFromTop(rowH);

    auto placeToggle = [&](juce::ToggleButton& btn)
    {
        btn.setBounds(row1.removeFromLeft(64));
    };
    placeToggle(loopBtn_);
    placeToggle(outputBtn_);
    placeToggle(followBtn_);
    placeToggle(previewBtn_);
    row1.removeFromLeft(8);

    auto placeSlider = [&](juce::Label& lbl, juce::Slider& sl)
    {
        lbl.setBounds(row1.removeFromLeft(60));
        sl.setBounds(row1.removeFromLeft(120));
        row1.removeFromLeft(8);
    };
    placeSlider(contrastLabel_,  contrastSlider_);
    placeSlider(brightnessLabel_,brightnessSlider_);

    // Row 2: snap + strategy + device
    row1 = area; // reuse remaining area as row2
    placeSlider(snapLabel_, snapSlider_);

    stratLabel_.setBounds(row1.removeFromLeft(50));
    snapStrategyBox_.setBounds(row1.removeFromLeft(80));
    row1.removeFromLeft(16);

    deviceLabel_.setBounds(row1.removeFromLeft(50));
    deviceBox_.setBounds(row1.removeFromLeft(160));
    row1.removeFromLeft(12);
    clearAllBtn_.setBounds(row1.removeFromLeft(70));
}

void OptionsPanel::buttonClicked(juce::Button* b)
{
    if (b == &clearAllBtn_)
    {
        if (onClearAll) onClearAll();
        return;
    }
    if (b == &loopBtn_)    options_.loop          = loopBtn_.getToggleState();
    if (b == &outputBtn_)  options_.output        = outputBtn_.getToggleState();
    if (b == &followBtn_)  options_.followServer  = followBtn_.getToggleState();
    if (b == &previewBtn_) options_.preview       = previewBtn_.getToggleState();
    if (onOptionsChanged) onOptionsChanged();
}

void OptionsPanel::sliderValueChanged(juce::Slider* s)
{
    if (s == &contrastSlider_)   options_.heatMapContrast   = (float)contrastSlider_.getValue();
    if (s == &brightnessSlider_) options_.heatMapBrightness = (float)brightnessSlider_.getValue();
    if (s == &snapSlider_)       options_.snapRange         = (float)snapSlider_.getValue();
    if (onOptionsChanged) onOptionsChanged();
}

void OptionsPanel::comboBoxChanged(juce::ComboBox* c)
{
    if (c == &snapStrategyBox_)
        options_.snapStrategy = (snapStrategyBox_.getSelectedId() == 1)
                                ? SnapStrategy::Slope : SnapStrategy::Value;

    if (c == &deviceBox_)
    {
        int  sel     = deviceBox_.getSelectedId();
        auto devices = juce::MidiOutput::getAvailableDevices();
        if (sel >= 2 && sel - 2 < (int)devices.size())
            options_.outputDeviceId = devices[sel - 2].identifier;
        else
            options_.outputDeviceId = "";
    }

    if (onOptionsChanged) onOptionsChanged();
}
