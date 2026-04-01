#include "HeatmapData.h"
#include <thread>
#include <cmath>
#include <algorithm>

void HeatmapData::invalidate(const ChordTimeline& timeline, const Options& opts)
{
    pendingChords_  = timeline.getChordsCopy();
    pendingOptions_ = opts;
    dirty_          = true;
    triggerAsyncUpdate();
}

void HeatmapData::handleAsyncUpdate()
{
    if (!dirty_) return;
    dirty_ = false;

    auto chords  = pendingChords_;
    auto opts    = pendingOptions_;
    auto self    = this;

    std::thread([chords = std::move(chords), opts, self]() mutable
    {
        auto result = std::make_shared<std::vector<ChordHeatmap>>();
        result->reserve(chords.size());

        const int bins = 1024;
        const double centsPerBin = 1200.0 * std::log2(opts.freqMax / opts.freqMin) / bins;
        const int toleranceBins  = (int)std::ceil(opts.snapRange / centsPerBin);

        for (int ci = 0; ci < (int)chords.size(); ++ci)
        {
            const auto& chord = chords[ci];

            std::vector<double> curFreqs, prevFreqs, nextFreqs;

            for (const auto& t : chord.tones)
                if (t.enabled) curFreqs.push_back(t.frequency);

            if (ci > 0)
                for (const auto& t : chords[ci - 1].tones)
                    if (t.enabled) prevFreqs.push_back(t.frequency);

            if (ci + 1 < (int)chords.size())
                for (const auto& t : chords[ci + 1].tones)
                    if (t.enabled) nextFreqs.push_back(t.frequency);

            ChordHeatmap hm;
            hm.chordId = chord.id;
            hm.numBins = bins;

            hm.curveCurrent = buildConsonanceCurve(curFreqs,
                                                    opts.freqMin, opts.freqMax,
                                                    bins, opts.numHarmonics);
            hm.curvePrev = prevFreqs.empty()
                ? std::vector<float>()
                : buildConsonanceCurve(prevFreqs, opts.freqMin, opts.freqMax,
                                       bins, opts.numHarmonics);
            hm.curveNext = nextFreqs.empty()
                ? std::vector<float>()
                : buildConsonanceCurve(nextFreqs, opts.freqMin, opts.freqMax,
                                       bins, opts.numHarmonics);

            // Island mask: bins within snapRange of a local max in curveCurrent.
            // If chord has no tones, mark everything as island (nothing to darken).
            hm.islandMask.assign(bins, false);
            if (curFreqs.empty())
            {
                std::fill(hm.islandMask.begin(), hm.islandMask.end(), true);
            }
            else
            {
                for (int i = 1; i < bins - 1; ++i)
                {
                    if (hm.curveCurrent[i] > hm.curveCurrent[i - 1] &&
                        hm.curveCurrent[i] > hm.curveCurrent[i + 1])
                    {
                        int lo = std::max(0, i - toleranceBins);
                        int hi = std::min(bins - 1, i + toleranceBins);
                        for (int j = lo; j <= hi; ++j)
                            hm.islandMask[j] = true;
                    }
                }
            }

            result->push_back(std::move(hm));
        }

        juce::MessageManager::callAsync([result, self]() mutable
        {
            std::atomic_store(&self->current_,
                std::const_pointer_cast<const std::vector<ChordHeatmap>>(result));
            if (self->onRebuilt)
                self->onRebuilt(std::const_pointer_cast<std::vector<ChordHeatmap>>(result));
        });

    }).detach();
}
