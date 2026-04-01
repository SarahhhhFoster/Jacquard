# JACQUARD

A VST3 / AU / AUv3 / CLAP / Standalone plugin for microtonal composition via Sethares consonance optimization.

---

## What and why

Historical tuning systems define a fixed set of pitch compromises — useful for frets, shared notation, and transposing instruments, but fundamentally a constraint. There is solid research (Sethares 1993) that models consonance between frequencies directly from their harmonic content. What if, instead of picking pitches from a predefined scale, we computed a set of *compatible* frequencies relative to whatever tones are already present?

Jacquard does this. Given a chord of existing tones, the Sethares model is applied across the audible frequency range to produce a **consonance spectrum** — a curve that peaks at frequencies that would sound consonant alongside what's already there. Tones snap to those peaks. Adjacent chords share context: the spectrum overlays the previous and next chord's contributions in separate colours so you can hear and see how a transition will sound before committing.

Performers in the continuous-pitch domain — string players, vocalists, trombonists — naturally drift to more consonant positions relative to their ensemble. Jacquard brings that intuition to composed music without any scale, temperament, or key in sight.

---

## Data model

```mermaid
erDiagram
    Jacquard ||--|| ChordTimeline : "has a"
    ChordTimeline ||--o{ Chord : contains
    Chord ||--o{ Tone : "is made of"
    Chord ||--|| Heatmap : "is represented by"
    Jacquard ||--|| Options : has
    Options ||--|| LoopDetails : has
    Options ||--|| SnapStrategy : has

    ChordTimeline {
        Chord[] chords
    }
    Chord {
        FullTimeCode start
        FullTimeCode stop
        Tone[] tones
    }
    Tone {
        FullTimeCode start
        FullTimeCode stop
        float frequency
        bool enabled
        Uuid sessionId
    }
    Heatmap {
        float[] curveCurrent "consonance from this chord's own tones — white/cyan"
        float[] curvePrev    "consonance from the preceding chord's tones — orange/red"
        float[] curveNext    "consonance from the following chord's tones — blue/purple"
        bool[]  islandMask   "true within snapRange cents of a local consonance peak"
    }
    Options {
        bool loop
        bool output
        bool followServer
        bool preview
        LoopDetails loopDetails
        float snapRange
        SnapStrategy snapStrategy
        float heatMapContrast
        float heatMapBrightness
        string outputDeviceId
        double gridStepPPQ
    }
    SnapStrategy {
        Slope  "finds local consonance maxima (parabolic sub-bin interpolation)"
        Value  "picks the highest consonance value within snapRange"
    }
```

Tone `start`/`stop` are always clamped to their chord's time bounds. `FullTimeCode` is a `double` in PPQ (quarter-note units).

---

## Features

### Consonance heatmap
- Background is a per-chord consonance spectrum computed via Sethares' 1993 model (harmonic partial series, pairwise roughness, normalised to [0, 1]).
- Three layers composited additively: **current chord** (white-cyan), **previous chord** (orange-red), **next chord** (blue-purple).
- Regions outside consonance islands — the valleys between peaks — are dimmed to 10% brightness so snap targets are visually clear.
- Heatmap recomputes only on tone add / remove / toggle and on chord add / remove, not on positional edits.

### Snapping
- **Consonance snap** (default): clicking in the heatmap snaps the tone's frequency to the nearest consonance peak within `snapRange` cents. Two strategies: *Slope* (finds local maxima, uses parabolic sub-bin interpolation) and *Value* (picks the highest value in range).
- **JI snap** (Ctrl+click): snaps to the nearest just-intonation interval relative to the chord's first tone.
- **Time grid snap**: all tone edges and chord edges snap to the active grid when within 3 px of a grid line.
- Hover tip shows the snapped target frequency (labelled `[snapped]` when it differs from the raw cursor position). A yellow guide line marks the snap target.

### Time grid
The narrow strip at the top of the timeline bar contains grid controls:

| Button | Effect |
|--------|--------|
| **½**  | Coarser grid (e.g. 16th → 8th) |
| **×2** | Finer grid (e.g. 16th → 32nd) |
| **3**  | Triplet division (⅓ beat; toggles) |
| **5**  | Quintuplet division (⅕ beat; toggles) |

Default is 16th notes. A single click in the heatmap creates a tone spanning exactly one subdivision.

### Chord timeline
- **Drag** on the timeline bar to create a chord (snapped to grid).
- **Drag body** to move a chord (preserves duration, snaps start to grid).
- **Drag left/right edge handles** to resize.
- **Cmd+click** a chord to copy it; **Cmd+click** empty space to paste with the click as the new start.
- **Right-click** a chord to delete it; double-click also works.
- Chords cannot start before t = 0; the view cannot scroll before t = 0.

### Tone editing
- **Click** in a chord region to place a tone (frequency snapped to nearest consonance peak, duration = one grid step).
- **Drag** to set a custom duration; stop position snaps to grid within 3 px.
- **Drag a tone** (body) to move it — time axis snaps to grid, frequency axis snaps to consonance peaks.
- **Drag tone edges** to resize.
- **Right-click** a tone to delete it.
- **Ctrl+click** a tone to toggle it enabled / disabled.
- Tones you own appear aqua; tones from other sessions appear grey.
- Frequency label appears on tones wide enough to accommodate it.

### Navigation

| Gesture | Action |
|---------|--------|
| Horizontal scroll | Pan the time view |
| **Cmd + scroll** | Zoom the frequency axis (Y), centred on the cursor |
| Resize window | Scales the whole view |

### Multi-instance IPC
- Instances on the same machine elect a server via a TCP bind-race on port 52847.
- All chord / tone mutations are broadcast to peers in real time (4-byte-length-prefixed JSON).
- Tones created by another session are shown greyed out; enabling them is session-local.
- On server resign, remaining instances re-elect with 50–200 ms random jitter.

### MPE output
- Outputs to any available MPE-capable MIDI device.
- Lower zone, channels 2–16, ±48 semitone pitch-bend range.
- Pitch-bend value precisely places the tone at its exact frequency relative to the nearest MIDI note.
- Setup messages (RPN 6 = 15, RPN 0 = 48) sent on device open.

### Preview
When **Preview** is enabled in the options panel, selecting a chord fires all its enabled tones over MPE for 800 ms.

### Standalone app
The standalone build adds:
- Transport bar (play / pause / stop, BPM display, bar:beat position).
- Sine-wave audio output of the currently active tones.

---

## Building

Requires: CMake ≥ 3.22, a C++17 toolchain, internet access on first build (FetchContent pulls JUCE 8.0.4 and clap-juce-extensions).

```bash
./build.sh          # Release
./build.sh Debug    # Debug
```

Artefacts land in `build/Jacquard_artefacts/<Release|Debug>/`.

### Install / uninstall

```bash
./install.sh        # copies VST3, AU, CLAP to ~/Library/Audio/Plug-Ins/…
./uninstall.sh      # removes them
```

---

## Sethares model constants

| Constant | Value | Role |
|----------|-------|------|
| `kDStar` | 0.24  | Frequency distance of maximum roughness (normalised) |
| `kS1`    | 0.021 | Slope scale factor 1 |
| `kS2`    | 19.0  | Slope scale factor 2 |
| `kB1`    | 3.51  | Rise exponent |
| `kB2`    | 5.75  | Fall exponent |

Roughness between two partials: `a1·a2·(e^(−kB1·y) − e^(−kB2·y))` where `y = s·(f2−f1)` and `s = kDStar / (kS1·f1 + kS2)`.

Consonance is `1 − (totalRoughness / maxRoughness)`, normalised over all scanned frequencies.
