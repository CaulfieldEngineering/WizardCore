# HallSandbox – Reverb Prototype

## 🎯 Objective

Build a modular, from-scratch hall-style reverb effect using JUCE and C++. This project is intended for DSP exploration, education, and rapid prototyping—not commercial release (yet). Focus is on transparent structure, modularity, and sound quality over GUI or cross-DAW polish.

---

## ✅ Requirements

### 1. Plugin Format
- [ ] JUCE Standalone application (initial target)
- [ ] Optional VST3 build for DAW testing

### 2. Signal Flow

```
Input →
  [ Pre-Delay ] →
  [ Early Reflections (Taps) ] →
  [ Series Diffusion (Allpass Filters) ] →
  [ Parallel Comb Filters + Damping ] →
  [ Wet Mix ] →
Dry/Wet Crossfade → Output
```

### 3. Parameters
- [ ] `Reverb Time` (affects comb feedback coefficients)
- [ ] `Pre-Delay` (ms)
- [ ] `Damping` (lowpass filter cutoff in comb feedback)
- [ ] `Diffusion` amount (optional, controls allpass feedback)
- [ ] `Wet/Dry Mix`
- [ ] `Output Gain` (optional)

### 4. Architecture
- [ ] Implement all filters/delays from scratch using JUCE primitives
- [ ] Modular, reusable C++ components:
  - `CombFilter`
  - `AllpassFilter`
  - `ReverbTank`
  - `Diffuser` (if broken out separately)
- [ ] Sample-rate and block-size aware
- [ ] Minimal use of JUCE DSP classes unless necessary
- [ ] Platform-independent design (macOS/Windows parity)

### 5. Debugging & Exploratory Tools
- [ ] Toggle stages (pre-delay, diffusion, tank)
- [ ] Optionally plot or print internal delay states
- [ ] Tail decay observation methods

---

## 📁 File Structure Proposal

```
/Source
  /ReverbCore
    CombFilter.h/.cpp
    AllpassFilter.h/.cpp
    ReverbTank.h/.cpp
    Diffuser.h/.cpp
    HallSandboxProcessor.h/.cpp
  PluginEditor.h/.cpp
  PluginProcessor.h/.cpp
```

---

## 🧪 MVP Acceptance Criteria

- [ ] Standalone app compiles and runs
- [ ] Clear reverb tail audible on input
- [ ] Real-time control of reverb time and mix
- [ ] Bypass works cleanly
- [ ] Sound is musically useful (subjective evaluation)

---

## 🔭 Stretch Goals

- [ ] LFO-based modulation of delay lines
- [ ] IR export for comparison with real halls
- [ ] Shimmer / pitch-shifted feedback experiment
- [ ] Basic unit tests on core filters

---

## 📌 Notes

- This is not a product yet.
- Focus on modular DSP architecture and experimentation.
- UX and polish will come later, if this evolves toward release.
