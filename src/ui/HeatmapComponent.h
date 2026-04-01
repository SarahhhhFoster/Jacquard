#pragma once
#include "../model/ChordTimeline.h"
#include "../model/Options.h"
#include "../engine/HeatmapData.h"
#include "../engine/SnapEngine.h"
#include "../ipc/SessionManager.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <functional>

// HeatmapComponent is the main editing canvas.
//
// Y axis: frequency (log scale), top = fMax, bottom = fMin.
// X axis: time (PPQ), left = viewStartPPQ, right = viewEndPPQ.
//
// The background is a rendered consonance heatmap.
// Tones are drawn as coloured rectangles on top.
// Chords are delineated by subtle vertical lines.

class HeatmapComponent : public juce::Component,
                         public juce::ScrollBar::Listener,
                         private juce::AsyncUpdater
{
public:
    HeatmapComponent(ChordTimeline& timeline,
                     Options&       options,
                     SessionManager& session);
    ~HeatmapComponent() override;

    // Called when the heatmap data has been recomputed.
    void onHeatmapRebuilt(std::shared_ptr<std::vector<ChordHeatmap>> data);

    // Sync view range from timeline bar (both share the same view).
    void setViewRange(double startPPQ, double endPPQ);
    double viewStartPPQ() const { return viewStart_; }
    double viewEndPPQ()   const { return viewEnd_;   }

    // Set the current playhead position for rendering.
    void setPlayheadPPQ(double ppq);

    // Selected chord (affects which chord tones are placed into).
    void setSelectedChord(const juce::Uuid& id);

    // Callbacks for edits
    std::function<void(const Chord&)>                                   onChordChanged;
    std::function<void(const juce::Uuid& chordId, const Tone&)>         onToneAdded;
    std::function<void(const juce::Uuid& chordId, const juce::Uuid&)>   onToneRemoved;
    std::function<void(const juce::Uuid& chordId, const juce::Uuid&, bool)> onToneToggled;

    // juce::Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    // juce::ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* bar, double newRange) override;

private:
    void handleAsyncUpdate() override;

    // Coordinate mapping
    double pixelToFreq(float y)  const;
    float  freqToPixel(double f) const;
    double pixelToPPQ(float x)   const;
    float  ppqToPixel(double ppq) const;

    // Rendering helpers
    void renderBackground(juce::Graphics& g);
    void renderChordDividers(juce::Graphics& g, const std::vector<Chord>& chords);
    void renderTones(juce::Graphics& g, const std::vector<Chord>& chords);
    void renderSnapIndicator(juce::Graphics& g);
    void renderPlayhead(juce::Graphics& g);
    void renderHoverTip(juce::Graphics& g);
    void renderJILines(juce::Graphics& g);

    // Snap helpers
    double snapPPQToGrid(double ppq) const;
    double snapFreqForChord(double freq, const juce::Uuid& chordId) const;

    // Hit testing
    Tone*  toneAtPixel(float x, float y, juce::Uuid& chordIdOut);
    Chord* chordAtPixel(float x);

    // Drag state
    enum class DragMode { None, PlacingTone, MovingTone, ResizingToneLeft, ResizingToneRight };
    DragMode   dragMode_{ DragMode::None };
    juce::Point<float> dragStart_;
    juce::Uuid dragChordId_, dragToneId_;
    Tone       dragToneOriginal_;
    bool       dragSnap_{ false };

    // For new tone placement (drag to set time extent)
    double     pendingToneFreq_{ 0.0 };
    double     pendingToneStart_{ 0.0 };
    double     snapIndicatorFreq_{ 0.0 };  // horizontal line drawn at snap target

    // Hover state
    juce::Point<float> hoverPos_;
    bool               showHover_{ false };
    juce::String       hoverTip_;
    bool               jiMode_{ false };     // Ctrl held = JI snap

    ChordTimeline&  timeline_;
    Options&        options_;
    SessionManager& session_;

    double viewStart_{ 0.0 };
    double viewEnd_{ 16.0 };
    double playheadPPQ_{ 0.0 };
    juce::Uuid selectedChordId_;

    // Rendered heatmap image (cached)
    juce::Image heatmapImage_;
    bool        imageDirty_{ true };
    std::shared_ptr<const std::vector<ChordHeatmap>> heatmapData_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeatmapComponent)
};
