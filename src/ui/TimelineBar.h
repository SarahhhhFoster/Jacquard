#pragma once
#include "../model/ChordTimeline.h"
#include "../model/Options.h"
#include "../ipc/SessionManager.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>

// TimelineBar is the strip above the heatmap.
// Top kGridStripH pixels: grid-snap controls.
// Remaining pixels: chord extents (create/move/resize).

class TimelineBar : public juce::Component
{
public:
    TimelineBar(ChordTimeline& timeline,
                Options&       options,
                SessionManager& session);
    ~TimelineBar() override = default;

    void setViewRange(double startPPQ, double endPPQ);
    double viewStartPPQ() const { return viewStart_; }
    double viewEndPPQ()   const { return viewEnd_;   }

    void setPlayheadPPQ(double ppq);
    void refresh() { repaint(); }

    std::function<void(const juce::Uuid&)> onChordSelected;
    std::function<void()>                  onResetToStart;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    double pixelToPPQ(float x)  const;
    float  ppqToPixel(double p) const;
    double snapToGrid(double ppq) const;
    double gridStep()           const;
    juce::String gridLabel()    const;

    enum class Zone { Body, LeftEdge, RightEdge };
    struct HitResult { int chordIdx; Zone zone; };
    std::optional<HitResult> hitTest(float x, const std::vector<Chord>& chords) const;

    // ── Grid state ────────────────────────────────────────────────────────────
    enum class GridMode { Normal, Triplet, Quintuplet };
    GridMode gridMode_   { GridMode::Normal };
    int      gridDivisor_{ 4 };  // divisions per beat in Normal mode; default=16th notes

    static constexpr int kGridStripH = 22; // px for button strip at top

    ChordTimeline&  timeline_;
    Options&        options_;
    SessionManager& session_;

    double viewStart_   = 0.0;
    double viewEnd_     = 16.0;
    double playheadPPQ_ = 0.0;

    enum class DragMode { None, Creating, Moving, ResizingLeft, ResizingRight };
    DragMode   dragMode_{ DragMode::None };
    float      dragStartX_{ 0.0f };
    juce::Uuid dragChordId_;
    double     dragChordOrigStart_{ 0.0 };
    double     dragChordOrigStop_{ 0.0 };

    std::optional<Chord> clipboard_;
    juce::Uuid selectedChordId_;
    float      lastMouseX_{ 0.0f };


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineBar)
};
