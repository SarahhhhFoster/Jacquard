#include "TransportPanel.h"

TransportPanel::TransportPanel()
{
    playPauseBtn_.addListener(this);
    stopBtn_.addListener(this);
    addAndMakeVisible(playPauseBtn_);
    addAndMakeVisible(stopBtn_);

    bpmLabel_.setFont(juce::Font(11.0f));
    bpmLabel_.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    addAndMakeVisible(bpmLabel_);

    bpmValue_.setEditable(true, true, false);
    bpmValue_.setText("120", juce::dontSendNotification);
    bpmValue_.setFont(juce::Font(11.0f, juce::Font::bold));
    bpmValue_.setColour(juce::Label::textColourId, juce::Colours::white);
    bpmValue_.setColour(juce::Label::backgroundColourId, juce::Colour(40, 40, 50));
    bpmValue_.setColour(juce::Label::outlineWhenEditingColourId, juce::Colours::cyan);
    bpmValue_.setJustificationType(juce::Justification::centred);
    bpmValue_.addListener(this);
    addAndMakeVisible(bpmValue_);

    posLabel_.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, 0));
    posLabel_.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    posLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(posLabel_);

    setIsPlaying(false);
    setPositionPPQ(0.0, 120.0);
}

TransportPanel::~TransportPanel()
{
    playPauseBtn_.removeListener(this);
    stopBtn_.removeListener(this);
    bpmValue_.removeListener(this);
}

void TransportPanel::setIsPlaying(bool playing)
{
    isPlaying_ = playing;
    playPauseBtn_.setButtonText(playing ? "Pause" : "Play");
}

void TransportPanel::setPositionPPQ(double ppq, double bpm)
{
    // Convert PPQ to bars:beats (4/4 assumed)
    int totalBeats = (int)std::max(0.0, ppq);
    int bar        = totalBeats / 4 + 1;
    int beat       = totalBeats % 4 + 1;

    posLabel_.setText(juce::String(bar) + ":" + juce::String(beat), juce::dontSendNotification);
    bpmValue_.setText(juce::String((int)bpm), juce::dontSendNotification);
}

void TransportPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(22, 22, 28));
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());
}

void TransportPanel::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    stopBtn_.setBounds(area.removeFromLeft(48));
    area.removeFromLeft(4);
    playPauseBtn_.setBounds(area.removeFromLeft(56));
    area.removeFromLeft(12);
    bpmLabel_.setBounds(area.removeFromLeft(28));
    bpmValue_.setBounds(area.removeFromLeft(40));
    area.removeFromLeft(12);
    posLabel_.setBounds(area.removeFromLeft(60));
}

void TransportPanel::buttonClicked(juce::Button* b)
{
    if (b == &playPauseBtn_)
    {
        isPlaying_ = !isPlaying_;
        setIsPlaying(isPlaying_);
        if (onPlayPauseChanged) onPlayPauseChanged(isPlaying_);
    }
    else if (b == &stopBtn_)
    {
        isPlaying_ = false;
        setIsPlaying(false);
        if (onPlayPauseChanged) onPlayPauseChanged(false);
        if (onStop)             onStop();
    }
}

void TransportPanel::labelTextChanged(juce::Label* lbl)
{
    if (lbl == &bpmValue_)
    {
        double bpm = bpmValue_.getText().getDoubleValue();
        bpm = juce::jlimit(20.0, 300.0, bpm);
        if (onBpmChanged) onBpmChanged(bpm);
    }
}
