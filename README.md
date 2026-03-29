# WizardCore

Shared DSP and utility library for Wonderland Audio plugin projects. Contains reusable C++ audio modules that are consumed by plugin repositories as a versioned dependency — no copy-pasting required.

---

## What's in here

All modules live under `plugin/source/` and are exposed under the `WizardCore` C++ namespace.

| Module | Path | Description |
|---|---|---|
| `LFO` | `plugin/source/LFO/` | Multi-waveform LFO with host sync, phase offset, symmetry control, and parameter smoothing |
| `DelayLine` | `plugin/source/DelayLine/` | Abstract delay line interface with two concrete implementations: `DigitalDelayLine` (clean) and `BBDelayLine` (bucket-brigade emulation). Factory methods: `DelayLine::create()`, `DelayLine::createDigital()`, `DelayLine::createBBD()` |
| `Chorus` | `plugin/source/Chorus/` | Multi-voice chorus built on top of `LFO` and `DelayLine`. Supports mono, stereo, and mid-side panning modes, per-voice control, and optional LPF/HPF filtering |

---

## How to use it in a plugin project

WizardCore is consumed via **CPM (CMake Package Manager)**. Add the following to your project's root `CMakeLists.txt`:

```cmake
CPMAddPackage(
    NAME WizardCore
    GIT_TAG staging
    GITHUB_REPOSITORY CaulfieldEngineering/WizardCore
    SOURCE_DIR ${LIB_DIR}/wizardcore
)
```

Then link against it in your plugin's `CMakeLists.txt`:

```cmake
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        WizardCore::AudioModules   # links all modules at once
)
```

Or link individual modules if you only need a subset:

```cmake
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        WizardCore::LFO
        WizardCore::DelayLine
)
```

### Available CMake targets

| Target | Contents |
|---|---|
| `WizardCore::LFO` | LFO module only |
| `WizardCore::DelayLine` | DelayLine module only |
| `WizardCore::Chorus` | Chorus module (pulls in LFO and DelayLine) |
| `WizardCore::AudioModules` | All modules (convenience umbrella target) |

---

## How to use the modules in C++

All modules follow the same pattern: construct, `prepare()`, then process.

```cpp
#include "LFO/LFO.h"
#include "DelayLine/DelayLine.h"
#include "Chorus/Chorus.h"

// LFO
WizardCore::LFO lfo;
lfo.prepare(sampleRate);
lfo.setFrequency(2.0);
lfo.setWaveShape(WizardCore::LFO::WaveShape::Triangle);
float value = lfo.getNextSample();  // call once per sample

// DelayLine (BBD emulation by default)
auto delay = WizardCore::DelayLine::create(WizardCore::DelayType::BBDelay);
delay->prepare(sampleRate, 1.0, numChannels);  // 1 second max delay
delay->setDelayTimeInSeconds(0.025);           // 25ms
delay->processBlock(audioBuffer);

// Chorus
WizardCore::Chorus chorus(5);  // up to 5 voices
chorus.prepare(sampleRate, numChannels);
chorus.setRate(1.0f);
chorus.setDepth(0.5f);
chorus.setMix(0.7f);
chorus.processBlock(audioBuffer);
```

---

## Dual-mode build

WizardCore's `CMakeLists.txt` automatically detects how it is being built:

- **Direct build** (`cmake -S . -B build` inside this repo): builds a full standalone/VST3 plugin. Useful for developing and auditioning modules in isolation.
- **Subproject mode** (imported via CPM by another repo): only the `WizardCore::` CMake targets are created. The full plugin is skipped.

Detection is based on whether `CMAKE_PROJECT_NAME == PROJECT_NAME`.

---

## Adding a new module

1. Create a folder under `plugin/source/YourModule/`
2. Add `YourModule.h` and `YourModule.cpp` — use the `WizardCore` namespace
3. Register it in `plugin/source/CMakeLists.txt` following the existing pattern (add an `INTERFACE` library target, alias it to `WizardCore::YourModule`, and add it to `WizardCore_AudioModules`)
4. Write tests (tests go in `test/` — currently scaffolded but not yet active)

---

## Project info

- **Company:** Mr. Wizard FX
- **CMake minimum:** 3.22
- **C++ standard:** C++23 (C++20 on macOS until upstream JUCE fix)
- **JUCE version:** 8.0.8
- **Branch convention:** `staging` is the main branch
