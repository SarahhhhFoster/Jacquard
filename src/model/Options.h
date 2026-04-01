#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

enum class SnapStrategy { Slope, Value };

struct LoopDetails
{
    double start = 0.0;
    double stop  = 4.0; // PPQ

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree vt("LoopDetails");
        vt.setProperty("start", start, nullptr);
        vt.setProperty("stop",  stop,  nullptr);
        return vt;
    }
    static LoopDetails fromValueTree(const juce::ValueTree& vt)
    {
        return { (double)vt["start"], (double)vt["stop"] };
    }
};

struct Options
{
    bool         loop             = false;
    bool         output           = false;
    bool         followServer     = true;
    bool         preview          = false;
    LoopDetails  loopDetails;
    float        snapRange        = 50.0f;  // cents — tolerance for snap search
    SnapStrategy snapStrategy     = SnapStrategy::Slope;
    float        heatMapContrast  = 1.0f;
    float        heatMapBrightness= 0.0f;
    juce::String outputDeviceId;            // MidiDeviceInfo::identifier

    // Frequency view range (Hz)
    double freqMin = 20.0;
    double freqMax = 8000.0;

    // View range (PPQ) — driven by UI zoom/pan
    double viewStartPPQ = 0.0;
    double viewEndPPQ   = 16.0;

    // How many harmonics to use in the Sethares model
    int numHarmonics = 6;

    // Grid step in PPQ (shared between TimelineBar and HeatmapComponent).
    // TimelineBar updates this when the user changes the grid.
    double gridStepPPQ = 0.25;  // default = 16th notes

    // How much weight the preceding and following chord's consonance curves
    // contribute to the snap target.  0 = current chord only; 1 = equal weight.
    float neighbourWeight = 0.0f;

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree vt("Options");
        vt.setProperty("loop",              loop,             nullptr);
        vt.setProperty("output",            output,           nullptr);
        vt.setProperty("followServer",      followServer,     nullptr);
        vt.setProperty("preview",           preview,          nullptr);
        vt.setProperty("snapRange",         snapRange,        nullptr);
        vt.setProperty("snapStrategy",      (int)snapStrategy,nullptr);
        vt.setProperty("heatMapContrast",   heatMapContrast,  nullptr);
        vt.setProperty("heatMapBrightness", heatMapBrightness,nullptr);
        vt.setProperty("outputDeviceId",    outputDeviceId,   nullptr);
        vt.setProperty("neighbourWeight",   neighbourWeight,  nullptr);
        vt.setProperty("freqMin",           freqMin,          nullptr);
        vt.setProperty("freqMax",           freqMax,          nullptr);
        vt.setProperty("viewStartPPQ",      viewStartPPQ,     nullptr);
        vt.setProperty("viewEndPPQ",        viewEndPPQ,       nullptr);
        vt.setProperty("numHarmonics",      numHarmonics,     nullptr);
        vt.addChild(loopDetails.toValueTree(), -1, nullptr);
        return vt;
    }

    static Options fromValueTree(const juce::ValueTree& vt)
    {
        Options o;
        o.loop              = (bool)   vt["loop"];
        o.output            = (bool)   vt["output"];
        o.followServer      = (bool)   vt["followServer"];
        o.preview           = (bool)   vt["preview"];
        o.snapRange         = (float)  vt["snapRange"];
        o.snapStrategy      = (SnapStrategy)(int)vt["snapStrategy"];
        o.heatMapContrast   = (float)  vt["heatMapContrast"];
        o.heatMapBrightness = (float)  vt["heatMapBrightness"];
        o.outputDeviceId    =          vt["outputDeviceId"].toString();
        o.neighbourWeight   = (float)  vt["neighbourWeight"];
        o.freqMin           = (double) vt["freqMin"];
        o.freqMax           = (double) vt["freqMax"];
        o.viewStartPPQ      = (double) vt["viewStartPPQ"];
        o.viewEndPPQ        = (double) vt["viewEndPPQ"];
        o.numHarmonics      = (int)    vt["numHarmonics"];
        auto ldTree = vt.getChildWithName("LoopDetails");
        if (ldTree.isValid())
            o.loopDetails = LoopDetails::fromValueTree(ldTree);
        return o;
    }
};
