#include "TimelineBar.h"
#include <algorithm>
#include <cmath>

TimelineBar::TimelineBar(ChordTimeline& timeline, Options& options, SessionManager& session)
    : timeline_(timeline), options_(options), session_(session)
{
    viewStart_ = options_.viewStartPPQ;
    viewEnd_   = options_.viewEndPPQ;
    options_.gridStepPPQ = gridStep();  // initialise shared state
    setOpaque(true);
}

void TimelineBar::setViewRange(double startPPQ, double endPPQ)
{
    viewStart_ = startPPQ;
    viewEnd_   = endPPQ;
    repaint();
}

void TimelineBar::setPlayheadPPQ(double ppq)
{
    playheadPPQ_ = ppq;
    repaint();
}

double TimelineBar::gridStep() const
{
    switch (gridMode_)
    {
        case GridMode::Normal:     return 1.0 / gridDivisor_;
        case GridMode::Triplet:    return 1.0 / 3.0;
        case GridMode::Quintuplet: return 1.0 / 5.0;
    }
    return 0.25;
}

double TimelineBar::snapToGrid(double ppq) const
{
    double step    = gridStep();
    double snapped = std::round(ppq / step) * step;
    return std::max(0.0, snapped);
}

juce::String TimelineBar::gridLabel() const
{
    switch (gridMode_)
    {
        case GridMode::Triplet:    return "3";
        case GridMode::Quintuplet: return "5";
        default: break;
    }
    // Normal mode: name based on gridDivisor_
    switch (gridDivisor_)
    {
        case 1:  return "1/4";
        case 2:  return "1/8";
        case 4:  return "1/16";
        case 8:  return "1/32";
        case 16: return "1/64";
        default: return "1/" + juce::String(gridDivisor_ * 4);
    }
}

double TimelineBar::pixelToPPQ(float x) const
{
    return viewStart_ + (x / getWidth()) * (viewEnd_ - viewStart_);
}

float TimelineBar::ppqToPixel(double p) const
{
    return (float)((p - viewStart_) / (viewEnd_ - viewStart_) * getWidth());
}

// ── Hit testing ───────────────────────────────────────────────────────────────

std::optional<TimelineBar::HitResult>
TimelineBar::hitTest(float x, const std::vector<Chord>& chords) const
{
    for (int i = 0; i < (int)chords.size(); ++i)
    {
        float x1 = ppqToPixel(chords[i].start);
        float x2 = ppqToPixel(chords[i].stop);
        if (x < x1 - 1 || x > x2 + 1) continue;

        if (x < x1 + 6) return HitResult{ i, Zone::LeftEdge  };
        if (x > x2 - 6) return HitResult{ i, Zone::RightEdge };
        return HitResult{ i, Zone::Body };
    }
    return std::nullopt;
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void TimelineBar::paint(juce::Graphics& g)
{
    int W = getWidth();
    int H = getHeight();

    g.fillAll(juce::Colour(25, 25, 30));

    // ── Grid button strip (top kGridStripH px) ────────────────────────────────
    g.setColour(juce::Colour(18, 18, 24));
    g.fillRect(0, 0, W, kGridStripH);

    // Buttons: coarser (1/2), finer (x2), triplet (3), quintuplet (5), reset (|<)
    struct BtnInfo { const char* label; int x; int w; bool active; };
    const BtnInfo btns[] = {
        { "1/2", 2,  22, false                             },
        { "x2",  26, 22, false                             },
        { "3",   52, 20, gridMode_ == GridMode::Triplet    },
        { "5",   74, 20, gridMode_ == GridMode::Quintuplet },
        { "|<",  96, 22, false                             },
    };

    g.setFont(10.0f);
    for (const auto& btn : btns)
    {
        juce::Colour bg = btn.active
            ? juce::Colour(100, 80, 180)
            : juce::Colour(50, 50, 65);
        g.setColour(bg);
        g.fillRoundedRectangle((float)btn.x, 2.0f, (float)btn.w, (float)kGridStripH - 4, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawText(btn.label, btn.x, 2, btn.w, kGridStripH - 4,
                   juce::Justification::centred, false);
    }

    // Current grid label
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawText(gridLabel(), 122, 2, 60, kGridStripH - 4,
               juce::Justification::centredLeft, false);

    // ── Chord area (below strip) ──────────────────────────────────────────────
    int chordY = kGridStripH;
    int chordH = H - kGridStripH;

    // Grid subdivision lines
    double step = gridStep();
    for (double ppq = std::ceil(viewStart_ / step) * step; ppq <= viewEnd_; ppq += step)
    {
        float x = ppqToPixel(ppq);
        bool isBeat = std::fmod(std::abs(ppq), 1.0) < 1e-7;
        bool isBar  = isBeat && std::fmod(std::abs(ppq), 4.0) < 1e-7;

        if (isBar)
        {
            g.setColour(juce::Colours::white.withAlpha(0.28f));
            g.drawVerticalLine((int)x, (float)chordY, (float)H);
            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.setFont(8.0f);
            g.drawText(juce::String((int)(ppq / 4) + 1),
                       (int)x + 2, chordY, 20, chordH, juce::Justification::centredLeft);
        }
        else if (isBeat)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.drawVerticalLine((int)x, (float)chordY, (float)H);
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawVerticalLine((int)x, (float)chordY, (float)H);
        }
    }

    auto chords = timeline_.getChordsCopy();

    // Draw chords
    for (const auto& c : chords)
    {
        float x1  = ppqToPixel(c.start);
        float x2  = ppqToPixel(c.stop);
        bool  sel = (c.id == selectedChordId_);

        juce::Colour fill = sel ? juce::Colour(80, 60, 140) : juce::Colour(50, 40, 90);
        g.setColour(fill.withAlpha(0.75f));
        g.fillRoundedRectangle(x1, (float)chordY + 2, x2 - x1, (float)chordH - 4, 4.0f);

        g.setColour(juce::Colours::white.withAlpha(sel ? 0.7f : 0.3f));
        g.drawRoundedRectangle(x1, (float)chordY + 2, x2 - x1, (float)chordH - 4, 4.0f, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillRect(x1,        (float)chordY + 2, 4.0f, (float)chordH - 4);
        g.fillRect(x2 - 4.0f, (float)chordY + 2, 4.0f, (float)chordH - 4);

        if (!c.tones.empty())
        {
            g.setFont(8.0f);
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.drawText(juce::String(c.tones.size()) + "t",
                       (int)x1 + 5, chordY, 20, chordH, juce::Justification::centredLeft);
        }
    }

    // Playhead
    float phX = ppqToPixel(playheadPPQ_);
    g.setColour(juce::Colours::yellow.withAlpha(0.8f));
    g.drawVerticalLine((int)phX, (float)chordY, (float)H);

    // Ghost chord during creation drag
    if (dragMode_ == DragMode::Creating)
    {
        float x1 = ppqToPixel(snapToGrid(pixelToPPQ(dragStartX_)));
        float x2 = ppqToPixel(snapToGrid(pixelToPPQ(lastMouseX_)));
        if (x1 > x2) std::swap(x1, x2);
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.fillRect(x1, (float)chordY + 2, x2 - x1, (float)chordH - 4);
    }
}

void TimelineBar::resized() {}

// ── Mouse handling ────────────────────────────────────────────────────────────

void TimelineBar::mouseDown(const juce::MouseEvent& e)
{
    lastMouseX_ = e.position.x;

    // Grid button strip — handle button clicks only
    if (e.position.y < kGridStripH)
    {
        float bx = e.position.x;
        if (bx >= 2 && bx < 24)
        {
            if (gridDivisor_ > 1) gridDivisor_ /= 2;
            gridMode_ = GridMode::Normal;
        }
        else if (bx >= 26 && bx < 48)
        {
            if (gridDivisor_ < 32) gridDivisor_ *= 2;
            gridMode_ = GridMode::Normal;
        }
        else if (bx >= 52 && bx < 72)
        {
            gridMode_ = (gridMode_ == GridMode::Triplet)
                      ? GridMode::Normal : GridMode::Triplet;
        }
        else if (bx >= 74 && bx < 94)
        {
            gridMode_ = (gridMode_ == GridMode::Quintuplet)
                      ? GridMode::Normal : GridMode::Quintuplet;
        }
        else if (bx >= 96 && bx < 118)
        {
            if (onResetToStart) onResetToStart();
        }
        options_.gridStepPPQ = gridStep();  // keep shared state in sync
        repaint();
        return;
    }

    auto chords = timeline_.getChordsCopy();
    auto hit    = hitTest(e.position.x, chords);

    if (hit)
    {
        const auto& chord = chords[hit->chordIdx];
        selectedChordId_  = chord.id;
        if (onChordSelected) onChordSelected(chord.id);

        // Right-click on chord = delete
        if (e.mods.isRightButtonDown())
        {
            session_.removeChord(chord.id);
            selectedChordId_ = {};
            if (onChordSelected) onChordSelected({});
            return;
        }

        if (e.mods.isCommandDown())
        {
            clipboard_ = chord;
            return;
        }

        dragChordId_         = chord.id;
        dragChordOrigStart_  = chord.start;
        dragChordOrigStop_   = chord.stop;
        dragStartX_          = e.position.x;

        switch (hit->zone)
        {
            case Zone::Body:       dragMode_ = DragMode::Moving;        break;
            case Zone::LeftEdge:   dragMode_ = DragMode::ResizingLeft;  break;
            case Zone::RightEdge:  dragMode_ = DragMode::ResizingRight; break;
        }
        return;
    }

    if (e.mods.isRightButtonDown()) return;

    if (e.mods.isCommandDown() && clipboard_)
    {
        Chord newChord = *clipboard_;
        newChord.id    = juce::Uuid();
        double dur     = newChord.stop - newChord.start;
        double pos     = snapToGrid(pixelToPPQ(e.position.x));
        double offset  = pos - newChord.start;
        newChord.start = pos;
        newChord.stop  = pos + dur;
        for (auto& t : newChord.tones)
        {
            t.id        = juce::Uuid();
            t.start    += offset;
            t.stop     += offset;
            t.sessionId = session_.sessionId();
        }
        session_.addChord(newChord);
        selectedChordId_ = newChord.id;
        if (onChordSelected) onChordSelected(newChord.id);
        return;
    }

    dragMode_   = DragMode::Creating;
    dragStartX_ = e.position.x;
}

void TimelineBar::mouseDrag(const juce::MouseEvent& e)
{
    lastMouseX_ = e.position.x;
    if (dragMode_ == DragMode::None || dragMode_ == DragMode::Creating)
    {
        repaint();
        return;
    }

    auto chord = timeline_.chordById(dragChordId_);
    if (!chord) return;

    double rawPPQ = pixelToPPQ(e.position.x);
    double rawStart = pixelToPPQ(dragStartX_);
    double dPPQ = rawPPQ - rawStart;
    double minDur = gridStep();

    if (dragMode_ == DragMode::Moving)
    {
        double newStart = std::max(0.0, snapToGrid(dragChordOrigStart_ + dPPQ));
        chord->stop  = newStart + (dragChordOrigStop_ - dragChordOrigStart_);
        chord->start = newStart;
    }
    else if (dragMode_ == DragMode::ResizingLeft)
    {
        chord->start = snapToGrid(juce::jlimit(0.0, chord->stop - minDur,
                                               dragChordOrigStart_ + dPPQ));
    }
    else if (dragMode_ == DragMode::ResizingRight)
    {
        chord->stop = snapToGrid(juce::jlimit(chord->start + minDur, 1e9,
                                              dragChordOrigStop_ + dPPQ));
    }

    for (auto& t : chord->tones)
        chord->clampTone(t);

    session_.updateChord(*chord);
    repaint();
}

void TimelineBar::mouseUp(const juce::MouseEvent& e)
{
    if (dragMode_ == DragMode::Creating)
    {
        double start = snapToGrid(pixelToPPQ(dragStartX_));
        double stop  = snapToGrid(pixelToPPQ(e.position.x));
        if (stop < start) std::swap(start, stop);

        if (stop - start >= gridStep())
        {
            Chord newChord;
            newChord.start = start;
            newChord.stop  = stop;
            session_.addChord(newChord);
            selectedChordId_ = newChord.id;
            if (onChordSelected) onChordSelected(newChord.id);
        }
    }

    dragMode_ = DragMode::None;
    repaint();
}

void TimelineBar::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.position.y < kGridStripH) return;
    auto chords = timeline_.getChordsCopy();
    auto hit    = hitTest(e.position.x, chords);
    if (hit)
    {
        session_.removeChord(chords[hit->chordIdx].id);
        selectedChordId_ = {};
        if (onChordSelected) onChordSelected({});
    }
}
