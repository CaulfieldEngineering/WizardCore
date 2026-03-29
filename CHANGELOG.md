# WizardCore Changelog

All notable changes to this project will be documented here.

Format: `vMAJOR.MINOR.PATCH`
- `PATCH` — bug fixes only, no behavioral or API changes
- `MINOR` — new features, new modules, or any DSP algorithm change (even improvements)
- `MAJOR` — breaking API changes requiring code updates in consumers

---

## v0.1.0 — Initial release

### Modules
- `LFO` — multi-waveform LFO with host sync, phase offset, symmetry, and parameter smoothing
- `DelayLine` — abstract delay interface with `DigitalDelayLine` and `BBDelayLine` implementations; factory pattern via `DelayLine::create()`
- `Chorus` — multi-voice chorus built on LFO and DelayLine; mono, stereo, and mid-side panning modes; optional LPF/HPF filtering
