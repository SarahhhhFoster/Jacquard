#include "HeatmapComponent.h"
#include <cmath>

// ── Colour constants ──────────────────────────────────────────────────────────
static const juce::Colour kColourOwnTone   = juce::Colour(0, 210, 190);
static const juce::Colour kColourOtherTone = juce::Colour(80, 80, 80);
static const juce::Colour kColourPlayhead  = juce::Colours::yellow.withAlpha(0.8f);
static const juce::Colour kColourJILine    = juce::Colours::white.withAlpha(0.15f);
static const juce::Colour kColourSnapLine  = juce::Colour(255, 220, 80).withAlpha(0.55f);

// ── Constructor / Destructor ──────────────────────────────────────────────────

HeatmapComponent::HeatmapComponent(ChordTimeline& timeline,
                                    Options&       options,
                                    SessionManager& session)
    : timeline_(timeline), options_(options), session_(session)
{
    viewStart_ = options_.viewStartPPQ;
    viewEnd_   = options_.viewEndPPQ;
    setOpaque(true);
}

HeatmapComponent::~HeatmapComponent() {}

// ── Public API ────────────────────────────────────────────────────────────────

void HeatmapComponent::onHeatmapRebuilt(std::shared_ptr<std::vector<ChordHeatmap>> data)
{
    heatmapData_ = data;
    imageDirty_  = true;
    repaint();
}

void HeatmapComponent::setViewRange(double startPPQ, double endPPQ)
{
    viewStart_  = startPPQ;
    viewEnd_    = endPPQ;
    imageDirty_ = true;
    repaint();
}

void HeatmapComponent::setPlayheadPPQ(double ppq)
{
    playheadPPQ_ = ppq;
    repaint();
}

void HeatmapComponent::setSelectedChord(const juce::Uuid& id)
{
    selectedChordId_ = id;
    repaint();
}

// ── Coordinate mapping ────────────────────────────────────────────────────────

double HeatmapComponent::pixelToFreq(float y) const
{
    float  h = (float)getHeight();
    double t = y / h;
    return std::exp(std::log(options_.freqMax) +
                    t * (std::log(options_.freqMin) - std::log(options_.freqMax)));
}

float HeatmapComponent::freqToPixel(double f) const
{
    float  h    = (float)getHeight();
    double logR = (std::log(options_.freqMax) - std::log(f)) /
                  (std::log(options_.freqMax) - std::log(options_.freqMin));
    return (float)(logR * h);
}

double HeatmapComponent::pixelToPPQ(float x) const
{
    float w = (float)getWidth();
    return viewStart_ + (x / w) * (viewEnd_ - viewStart_);
}

float HeatmapComponent::ppqToPixel(double ppq) const
{
    float w = (float)getWidth();
    return (float)((ppq - viewStart_) / (viewEnd_ - viewStart_) * w);
}

// Snap ppq to grid if within 3 px of a grid line.
double HeatmapComponent::snapPPQToGrid(double ppq) const
{
    double step = options_.gridStepPPQ;
    if (step <= 0.0) return ppq;
    double snapped = std::round(ppq / step) * step;
    double ppqPer3px = 3.0 * (viewEnd_ - viewStart_) / std::max(1, getWidth());
    return (std::abs(ppq - snapped) < ppqPer3px) ? snapped : ppq;
}

// Snap frequency using the heatmap curve for the given chord (if available).
double HeatmapComponent::snapFreqForChord(double freq, const juce::Uuid& chordId) const
{
    if (!heatmapData_) return freq;
    for (const auto& hm : *heatmapData_)
    {
        if (hm.chordId == chordId && !hm.curveCurrent.empty())
            return snapFrequency(freq, hm.curveCurrent,
                                 options_.freqMin, options_.freqMax, options_);
    }
    return freq;
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void HeatmapComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(10, 10, 15));

    auto chords = timeline_.getChordsCopy();

    renderBackground(g);
    renderJILines(g);
    renderChordDividers(g, chords);
    renderTones(g, chords);
    renderSnapIndicator(g);
    renderPlayhead(g);
    if (showHover_) renderHoverTip(g);
}

void HeatmapComponent::renderBackground(juce::Graphics& g)
{
    if (!heatmapData_ || heatmapData_->empty()) return;

    int W = getWidth();
    int H = getHeight();

    if (imageDirty_ || heatmapImage_.getWidth() != W || heatmapImage_.getHeight() != H)
    {
        heatmapImage_ = juce::Image(juce::Image::ARGB, W, H, false);
        juce::Image::BitmapData bmp(heatmapImage_, juce::Image::BitmapData::writeOnly);

        auto chords    = timeline_.getChordsCopy();
        float contrast   = options_.heatMapContrast;
        float brightness = options_.heatMapBrightness;

        for (int x = 0; x < W; ++x)
        {
            double ppq = pixelToPPQ((float)x + 0.5f);

            const ChordHeatmap* hm = nullptr;
            for (const auto& chord : chords)
            {
                if (chord.containsPPQ(ppq))
                {
                    for (const auto& chm : *heatmapData_)
                        if (chm.chordId == chord.id) { hm = &chm; break; }
                    break;
                }
            }

            for (int y = 0; y < H; ++y)
            {
                float curV = 0.0f, prevV = 0.0f, nextV = 0.0f;

                if (hm && hm->numBins > 0)
                {
                    // bin 0 = freqMin (bottom), bin N-1 = freqMax (top)
                    // pixel y=0 = top = freqMax, y=H = bottom = freqMin
                    // both axes are log-spaced with the same freqMin/freqMax,
                    // so the mapping is simply: bin = (1 - y/H) * (N-1)
                    int   bin      = juce::jlimit(0, hm->numBins - 1,
                                         (int)std::round((1.0 - (double)y / H)
                                                         * (hm->numBins - 1)));
                    bool  inIsland = hm->islandMask.empty() || hm->islandMask[bin];
                    float dimFactor = inIsland ? 1.0f : 0.10f;

                    auto applyBC = [&](float v) -> float {
                        return juce::jlimit(0.0f, 1.0f,
                                            (v - 0.5f) * contrast + 0.5f + brightness);
                    };

                    if (!hm->curveCurrent.empty())
                        curV  = applyBC(hm->curveCurrent[bin]) * dimFactor;
                    if (!hm->curvePrev.empty())
                        prevV = applyBC(hm->curvePrev[bin]) * dimFactor;
                    if (!hm->curveNext.empty())
                        nextV = applyBC(hm->curveNext[bin]) * dimFactor;
                }

                // Additive compositing: current=white-cyan, prev=orange-red, next=blue-purple
                int r  = juce::jlimit(0, 255, (int)(curV * 160 + prevV * 255));
                int gv = juce::jlimit(0, 255, (int)(curV * 160 + prevV * 80 + nextV * 100));
                int b  = juce::jlimit(0, 255, (int)(curV * 210 + nextV * 255));

                bmp.setPixelColour(x, y, juce::Colour((uint8_t)r, (uint8_t)gv, (uint8_t)b));
            }
        }
        imageDirty_ = false;
    }

    g.drawImageAt(heatmapImage_, 0, 0);
}

void HeatmapComponent::renderJILines(juce::Graphics& g)
{
    static const double kCFreqs[]  = { 32.70, 65.41, 130.81, 261.63, 523.25, 1046.5, 2093.0, 4186.0 };
    static const char*  kCLabels[] = { "C1","C2","C3","C4","C5","C6","C7","C8" };

    g.setColour(kColourJILine);
    g.setFont(9.0f);
    int W = getWidth();

    for (int i = 0; i < 8; ++i)
    {
        float y = freqToPixel(kCFreqs[i]);
        if (y < 0 || y > getHeight()) continue;
        g.drawHorizontalLine((int)y, 0.0f, (float)W);
        g.drawText(kCLabels[i], 2, (int)y - 8, 20, 8, juce::Justification::left, false);
    }
}

void HeatmapComponent::renderChordDividers(juce::Graphics& g, const std::vector<Chord>& chords)
{
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    for (const auto& c : chords)
    {
        float x1 = ppqToPixel(c.start);
        float x2 = ppqToPixel(c.stop);
        g.drawVerticalLine((int)x1, 0.0f, (float)getHeight());
        g.drawVerticalLine((int)x2 - 1, 0.0f, (float)getHeight());

        if (c.id == selectedChordId_)
        {
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.fillRect(x1, 0.0f, x2 - x1, (float)getHeight());
            g.setColour(juce::Colours::white.withAlpha(0.2f));
        }
    }
}

void HeatmapComponent::renderTones(juce::Graphics& g, const std::vector<Chord>& chords)
{
    const float toneRadius = 6.0f;

    for (const auto& chord : chords)
    {
        bool isCurrent = (chord.id == selectedChordId_);

        for (const auto& tone : chord.tones)
        {
            float x1 = ppqToPixel(tone.start);
            float x2 = ppqToPixel(tone.stop);
            float y  = freqToPixel(tone.frequency);

            x1 = std::max(x1, 0.0f);
            x2 = std::min(x2, (float)getWidth());
            if (x2 < 0 || x1 > getWidth()) continue;

            juce::Colour col;
            bool isOwnSession = (tone.sessionId == session_.sessionId());

            if (isCurrent)
                col = isOwnSession ? kColourOwnTone : kColourOtherTone;
            else
                col = kColourOtherTone.withAlpha(0.5f);

            if (!tone.enabled)
                col = col.withAlpha(col.getFloatAlpha() * 0.35f);

            float barH = toneRadius * 2.0f;
            g.setColour(col);
            g.fillRoundedRectangle(x1, y - toneRadius, x2 - x1, barH, 4.0f);

            if (x2 - x1 > 40)
            {
                g.setColour(juce::Colours::black.withAlpha(0.7f));
                g.setFont(9.0f);
                g.drawText(juce::String(tone.frequency, 1) + " Hz",
                           (int)x1 + 2, (int)(y - toneRadius), (int)(x2 - x1) - 4,
                           (int)barH, juce::Justification::centredLeft, true);
            }

            g.setColour(col.brighter(0.4f));
            g.fillRect(x1,        y - toneRadius, 4.0f, barH);
            g.fillRect(x2 - 4.0f, y - toneRadius, 4.0f, barH);
        }
    }
}

// Draw a horizontal line at the pending snap frequency while placing a tone.
void HeatmapComponent::renderSnapIndicator(juce::Graphics& g)
{
    if (dragMode_ != DragMode::PlacingTone && !showHover_) return;
    if (snapIndicatorFreq_ <= 0.0) return;

    float y = freqToPixel(snapIndicatorFreq_);
    if (y < 0 || y > getHeight()) return;

    g.setColour(kColourSnapLine);
    g.drawHorizontalLine((int)y, 0.0f, (float)getWidth());
}

void HeatmapComponent::renderPlayhead(juce::Graphics& g)
{
    float x = ppqToPixel(playheadPPQ_);
    if (x < 0 || x > getWidth()) return;
    g.setColour(kColourPlayhead);
    g.drawVerticalLine((int)x, 0.0f, (float)getHeight());
    juce::Path tri;
    tri.addTriangle(x, 0, x - 5, -8, x + 5, -8);
    g.fillPath(tri);
}

void HeatmapComponent::renderHoverTip(juce::Graphics& g)
{
    if (hoverTip_.isEmpty()) return;
    g.setFont(10.0f);
    int tw = g.getCurrentFont().getStringWidth(hoverTip_) + 8;
    float tx = hoverPos_.x + 10;
    float ty = hoverPos_.y - 18;
    tx = juce::jlimit(0.0f, (float)(getWidth()  - tw), tx);
    ty = juce::jlimit(0.0f, (float)(getHeight() - 16), ty);

    g.setColour(juce::Colour(20, 20, 25).withAlpha(0.9f));
    g.fillRoundedRectangle(tx, ty, (float)tw, 14.0f, 3.0f);
    g.setColour(juce::Colours::white);
    g.drawText(hoverTip_, (int)tx + 4, (int)ty, tw - 8, 14, juce::Justification::centredLeft);
}

// ── Mouse handling ────────────────────────────────────────────────────────────

void HeatmapComponent::mouseMove(const juce::MouseEvent& e)
{
    hoverPos_  = e.position;
    showHover_ = true;
    jiMode_    = e.mods.isCtrlDown();

    double freq = pixelToFreq(e.position.y);
    double ppq  = pixelToPPQ(e.position.x);

    static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

    if (jiMode_)
    {
        double ref = 0.0;
        if (auto chord = timeline_.chordById(selectedChordId_))
            if (!chord->tones.empty())
                ref = chord->tones[0].frequency;

        auto ji = snapToJustIntonation(freq, ref);
        snapIndicatorFreq_ = ji.frequency;
        hoverTip_ = juce::String(ji.frequency, 1) + " Hz  " + ji.name;
    }
    else
    {
        // Apply consonance snap and show result
        double snapped = freq;
        auto chord = timeline_.chordAt(ppq);
        if (chord)
            snapped = snapFreqForChord(freq, chord->id);

        snapIndicatorFreq_ = snapped;

        auto mp     = freqToMidi(snapped);
        int  octave = mp.note / 12 - 1;
        auto name   = juce::String(noteNames[mp.note % 12]) + juce::String(octave);
        hoverTip_   = juce::String(snapped, 1) + " Hz  " + name
                    + "  " + (mp.cents >= 0 ? "+" : "") + juce::String(mp.cents, 0) + "¢";

        if (std::abs(snapped - freq) > 0.5)
            hoverTip_ += "  [snapped]";
    }
    repaint();
}

void HeatmapComponent::mouseDown(const juce::MouseEvent& e)
{
    // Right-click: delete tone under cursor
    if (e.mods.isRightButtonDown())
    {
        juce::Uuid chordId;
        if (toneAtPixel(e.position.x, e.position.y, chordId) != nullptr)
            session_.removeTone(dragChordId_, dragToneId_);
        return;
    }

    juce::Uuid chordId;
    Tone* hitTone = toneAtPixel(e.position.x, e.position.y, chordId);

    if (hitTone && e.mods.isCommandDown())
    {
        session_.toggleTone(chordId, hitTone->id, !hitTone->enabled);
        return;
    }

    if (hitTone)
    {
        float x1 = ppqToPixel(hitTone->start);
        float x2 = ppqToPixel(hitTone->stop);
        dragToneOriginal_ = *hitTone;
        dragToneId_       = hitTone->id;
        dragChordId_      = chordId;
        dragStart_        = e.position;

        if (e.position.x < x1 + 6)
            dragMode_ = DragMode::ResizingToneLeft;
        else if (e.position.x > x2 - 6)
            dragMode_ = DragMode::ResizingToneRight;
        else
            dragMode_ = DragMode::MovingTone;
        return;
    }

    // No hit — start placing a new tone
    double ppq  = pixelToPPQ(e.position.x);
    auto chord = timeline_.chordAt(ppq);
    if (!chord) return;

    selectedChordId_ = chord->id;
    dragChordId_     = chord->id;
    dragStart_       = e.position;
    dragMode_        = DragMode::PlacingTone;

    double freq = pixelToFreq(e.position.y);
    if (e.mods.isCtrlDown())
    {
        double ref = chord->tones.empty() ? 0.0 : chord->tones[0].frequency;
        freq = snapToJustIntonation(freq, ref).frequency;
    }
    else
    {
        freq = snapFreqForChord(freq, chord->id);
    }

    // Snap time start to grid
    pendingToneFreq_  = freq;
    pendingToneStart_ = snapPPQToGrid(ppq);
    snapIndicatorFreq_ = freq;
}

void HeatmapComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragMode_ == DragMode::None) return;

    if (dragMode_ == DragMode::PlacingTone)
    {
        // Update snap indicator position while dragging out the time range
        snapIndicatorFreq_ = pendingToneFreq_;
        repaint();
        return;
    }

    auto chord = timeline_.chordById(dragChordId_);
    if (!chord) return;

    auto* tone = chord->findTone(dragToneId_);
    if (!tone) return;

    double rawPPQ  = pixelToPPQ(e.position.x);
    double rawFreq = pixelToFreq(e.position.y);
    double dPPQ    = rawPPQ - pixelToPPQ(dragStart_.x);

    if (dragMode_ == DragMode::MovingTone)
    {
        // Time: snap to grid within 3px
        double newStart = snapPPQToGrid(dragToneOriginal_.start + dPPQ);
        double dur      = dragToneOriginal_.stop - dragToneOriginal_.start;
        tone->start  = newStart;
        tone->stop   = newStart + dur;

        // Frequency: apply consonance snap
        double rawF  = juce::jlimit(options_.freqMin, options_.freqMax,
                                    dragToneOriginal_.frequency
                                    + (rawFreq - pixelToFreq(dragStart_.y)));
        tone->frequency = snapFreqForChord(rawF, chord->id);

        chord->clampTone(*tone);
        session_.updateChord(*chord);
    }
    else if (dragMode_ == DragMode::ResizingToneLeft)
    {
        double snapped = snapPPQToGrid(dragToneOriginal_.start + dPPQ);
        tone->start = juce::jlimit(chord->start, tone->stop - options_.gridStepPPQ, snapped);
        session_.updateChord(*chord);
    }
    else if (dragMode_ == DragMode::ResizingToneRight)
    {
        double snapped = snapPPQToGrid(dragToneOriginal_.stop + dPPQ);
        tone->stop = juce::jlimit(tone->start + options_.gridStepPPQ, chord->stop, snapped);
        session_.updateChord(*chord);
    }

    repaint();
}

void HeatmapComponent::mouseUp(const juce::MouseEvent& e)
{
    if (dragMode_ == DragMode::PlacingTone)
    {
        auto chord = timeline_.chordById(dragChordId_);
        if (chord)
        {
            double startPPQ = pendingToneStart_;
            double stopPPQ  = snapPPQToGrid(pixelToPPQ(e.position.x));

            // Single click (no meaningful drag): span exactly one grid step
            float dragPx = std::abs(e.position.x - dragStart_.x);
            if (dragPx < 4.0f || std::abs(stopPPQ - startPPQ) < options_.gridStepPPQ * 0.5)
            {
                stopPPQ = startPPQ + options_.gridStepPPQ;
            }

            if (stopPPQ < startPPQ) std::swap(startPPQ, stopPPQ);

            if (stopPPQ - startPPQ > 0.0)
            {
                Tone t;
                t.frequency = pendingToneFreq_;
                t.start     = startPPQ;
                t.stop      = stopPPQ;
                t.sessionId = session_.sessionId();
                chord->clampTone(t);
                if (t.stop > t.start)
                    session_.addTone(chord->id, t);
            }
        }
    }

    snapIndicatorFreq_ = 0.0;
    dragMode_ = DragMode::None;
    repaint();
}

void HeatmapComponent::mouseWheelMove(const juce::MouseEvent& e,
                                       const juce::MouseWheelDetails& w)
{
    if (e.mods.isAltDown())
    {
        // Option+scroll  →  time-domain zoom, centred on cursor's PPQ position
        double pivotPPQ  = pixelToPPQ(e.position.x);
        double range     = viewEnd_ - viewStart_;
        double pivotFrac = juce::jlimit(0.0, 1.0, (pivotPPQ - viewStart_) / range);

        double delta    = (std::abs(w.deltaY) >= std::abs(w.deltaX)) ? w.deltaY : w.deltaX;
        double factor   = std::pow(0.9, delta * 3.0);
        double newRange = juce::jlimit(0.25, 256.0, range * factor);

        viewStart_ = std::max(0.0, pivotPPQ - pivotFrac * newRange);
        viewEnd_   = viewStart_ + newRange;

        options_.viewStartPPQ = viewStart_;
        options_.viewEndPPQ   = viewEnd_;
        imageDirty_ = true;
        repaint();
        return;
    }

    if (e.mods.isCommandDown())
    {
        // Cmd+scroll  →  frequency-domain zoom, centred on the cursor's frequency
        double pivotFreq = std::max(pixelToFreq(e.position.y), 1.0);
        double logMin    = std::log(options_.freqMin);
        double logMax    = std::log(options_.freqMax);
        double logRange  = logMax - logMin;
        double pivotFrac = juce::jlimit(0.0, 1.0,
                                        (std::log(pivotFreq) - logMin) / logRange);

        double delta       = (std::abs(w.deltaY) >= std::abs(w.deltaX)) ? w.deltaY : -w.deltaX;
        double factor      = std::pow(0.9, delta * 5.0);
        double newLogRange = juce::jlimit(1.0, 9.21, logRange * factor);

        double newLogMin = std::log(pivotFreq) - pivotFrac * newLogRange;
        double newLogMax = newLogMin + newLogRange;

        options_.freqMin = juce::jlimit(10.0, 8000.0,  std::exp(newLogMin));
        options_.freqMax = juce::jlimit(50.0, 20000.0, std::exp(newLogMax));
        if (options_.freqMax < options_.freqMin * 2.0)
            options_.freqMax = options_.freqMin * 2.0;

        if (onFreqRangeChanged) onFreqRangeChanged();
        imageDirty_ = true;
        repaint();
        return;
    }

    // Vertical scroll  →  frequency pan (shift the visible freq range up/down)
    if (std::abs(w.deltaY) > std::abs(w.deltaX))
    {
        double logMin   = std::log(options_.freqMin);
        double logMax   = std::log(options_.freqMax);
        double logRange = logMax - logMin;
        double shift    = -w.deltaY * logRange * 0.06;

        double newLogMin = juce::jlimit(std::log(10.0),    std::log(19000.0), logMin + shift);
        double newLogMax = newLogMin + logRange;
        if (newLogMax <= std::log(20000.0))
        {
            options_.freqMin = std::exp(newLogMin);
            options_.freqMax = std::exp(newLogMax);
            if (onFreqRangeChanged) onFreqRangeChanged();
        }
        imageDirty_ = true;
        repaint();
        return;
    }

    // Horizontal scroll  →  time pan; clamp so we can't scroll before t=0
    double viewRange = viewEnd_ - viewStart_;
    double delta     = -w.deltaX * viewRange * 0.1;
    viewStart_ += delta;
    viewEnd_   += delta;

    if (viewStart_ < 0.0)
    {
        viewEnd_   -= viewStart_;
        viewStart_  = 0.0;
    }

    options_.viewStartPPQ = viewStart_;
    options_.viewEndPPQ   = viewEnd_;
    imageDirty_ = true;
    repaint();
}

void HeatmapComponent::resized()
{
    imageDirty_ = true;
}

void HeatmapComponent::handleAsyncUpdate()
{
    repaint();
}

void HeatmapComponent::scrollBarMoved(juce::ScrollBar*, double) {}

// ── Hit testing ───────────────────────────────────────────────────────────────

Tone* HeatmapComponent::toneAtPixel(float x, float y, juce::Uuid& chordIdOut)
{
    const float toneRadius = 7.0f;
    auto chordsCopy = timeline_.getChordsCopy();

    for (auto& chord : chordsCopy)
    {
        for (auto& tone : chord.tones)
        {
            float ty  = freqToPixel(tone.frequency);
            float tx1 = ppqToPixel(tone.start);
            float tx2 = ppqToPixel(tone.stop);

            if (x >= tx1 && x <= tx2 && std::abs(y - ty) < toneRadius + 2)
            {
                chordIdOut   = chord.id;
                dragToneId_  = tone.id;
                dragChordId_ = chord.id;

                static Tone sentinel;
                sentinel = tone;
                return &sentinel;
            }
        }
    }
    return nullptr;
}
