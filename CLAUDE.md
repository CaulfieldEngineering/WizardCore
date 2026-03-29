# WizardCore — LLM Orientation

## What this repo is

WizardCore is the **shared DSP and utility library** for Wonderland Audio plugin projects. It is not a shipping product — it is a dependency consumed by plugin repos.

The authoritative standards for how this library fits into the broader ecosystem live in the **WLA Playbook**, specifically `03-development-pipeline.md` (Shared Code Library section).

## Role in the ecosystem

```
WizardCore  ←──(CPM)──  Plugin repos (e.g. ChorusSandbox, Tertiary, new products)
                              ↑
                    NewPluginTemplate (already wired to consume WizardCore)
```

When a new plugin is created from NewPluginTemplate, WizardCore is already linked as a dependency. Any reusable DSP logic should live here, not be copy-pasted into plugin source trees.

## What's in here

All modules are under `plugin/source/` and live in the `WizardCore` C++ namespace.

| Module | Description |
|---|---|
| `LFO` | Multi-waveform LFO, host sync, phase offset, parameter smoothing |
| `DelayLine` | Abstract delay interface with `DigitalDelayLine` and `BBDelayLine` implementations. Factory: `DelayLine::create(DelayType::BBDelay)` |
| `Chorus` | Multi-voice chorus built on LFO + DelayLine. Mono/stereo/mid-side modes. |

## CMake interface

Targets follow the `WizardCore::` namespace convention:
- `WizardCore::LFO`, `WizardCore::DelayLine`, `WizardCore::Chorus`
- `WizardCore::AudioModules` — umbrella target, links all modules at once

## Dual-mode build

The CMakeLists.txt detects how it is being built:
- **Direct build** (`cmake -S . -B build` inside this repo): builds a full standalone/VST3 plugin. Use this to develop and test modules in isolation.
- **Subproject mode** (imported via CPM by another repo): only the `WizardCore::` targets are created. The full plugin is skipped.

Detection: `if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)` — equal means direct build.

## How to add a new module

1. Create `plugin/source/YourModule/YourModule.h` and `YourModule.cpp`
2. Use the `WizardCore` C++ namespace
3. Register it in `plugin/source/CMakeLists.txt`:
   - Add an `INTERFACE` library target
   - Alias it to `WizardCore::YourModule`
   - Add it to `WizardCore_AudioModules`
4. Write unit tests in `test/` (currently scaffolded, not yet active)

## C++ usage pattern

All modules follow: construct → `prepare()` → process.

```cpp
#include "Chorus/Chorus.h"   // Include root is plugin/source/, NOT WizardCore/Chorus/...
#include "LFO/LFO.h"
#include "DelayLine/DelayLine.h"

WizardCore::LFO lfo;
lfo.prepare(sampleRate);
lfo.setFrequency(2.0);
float value = lfo.getNextSample(); // call once per sample
```

**Include path convention:** The CMake targets set `plugin/source/` as the include directory. Use `Module/Module.h`, never `WizardCore/Module/Module.h`.

## Key conventions

- Thread-safe and real-time-safe design is required for all modules
- Each module has a `Config` struct for parameter state (atomic members)
- `prepare(sampleRate, ...)` must be called before any audio processing
- Parameter changes go through setter methods, not direct struct mutation
- C++23 (C++20 on macOS)

## Branch convention

`staging` is the main branch.
