# Audited Capicola capability set

This is the controlling capability boundary for the first disting NT release. The machine-readable ledger is [`capability-set.json`](capability-set.json). Future implementation may constrain a listed capability further when measured NT feasibility requires it, but it must not add a processing or sampler capability without a new upstream audit and the review required by the approved Spec.

## Pinned evidence baseline

| Component | Pinned revision | Meaning |
|---|---|---|
| Capicola | `f0fb61cfa7111067b4ec1a642d1b16a0910adb3b` (`main`, no upstream release tag) | Audited DSP, panel labels/ranges, and analysis signals |
| distingNT_API | tag `v1.16.0`, commit `cd12d876dbe060859828053efab1cbc98c9df251` | Delivery SDK baseline |
| disting NT firmware | exactly `1.16.0` | Delivery/runtime baseline; no other firmware is implied supported |
| Plug-in API | `kNT_apiVersion13` | Custom UI, serialization, asynchronous WAV reading, 64-bus constants, and dynamic parameter-page support |
| Audio rate | 48 kHz | Capicola constants and all Hz/ms conversions in this audit |

Both repositories are Git submodules under `vendor/`; the gitlinks, not branch names, are authoritative. The SDK's `v1.15.0` and `v1.16.0` tags currently point to the same commit, so the exact commit is recorded to remove tag ambiguity. Firmware 1.16.0 still requires later ARM, emulator, and physical-runtime verification before release; this audit does not claim those later acceptance checks.

## First-release processing surface

The included user-facing processing capabilities are:

- **Pitch:** -12 to +12 semitones, with zero detent.
- **Stretch:** real time to freeze, preserving the upstream `(1-x)^2.5` taper.
- **Threshold:** adaptive transient ratio 0–8; the top disables automatic triggers.
- **Grain Size:** 32–4096 keyframes.
- **Quality:** analyzer epsilon 0.1 (coarse) to 0.001 (fine).
- **Feedback:** loop gain 0–1.5; above 1 is deliberately unstable and upstream clamps injection to +/-1.
- **Envelope Smoothing:** normalized cutoff 0.00005–0.125 (about 1.2 Hz–3 kHz at 48 kHz).
- **Fade:** 10–250 ms; one value remains crossfade, wet latency, and trigger-refractory floor.
- **Drive:** 0.5–4.0, applied to keyframes.
- **Drive Character:** quake through clean to sinc.
- **Mix:** dry to wet, retaining the upstream wet-path delay behavior.
- **Feedback Tone:** normalized bandpass center 0.002–0.9 (about 48 Hz–21.6 kHz at 48 kHz).
- **Slice:** momentary manual splice action.

Every control is stereo-linked, and ordinary modulation uses NT parameter mapping. The JSON ledger records a source symbol and an NT-specific constraint for every item.

## Audited analysis outputs

Four upstream signals are eligible for optional output assignment: **Input Transient**, **Output Transient**, **Input Envelope**, and **Output Envelope**. Transients are 0/5 V and envelopes are 0–5 V. Each future selector must include and default to `0`/disconnected. The output transient is analysis-only and never splices the engine.

This count fits the pinned platform's eight physical output buses, but allocation remains optional and must coexist with the two audio outputs. No bus is claimed by default.

## Sample-source boundary

Loaded-sample mode is an approved NT wrapper source adapter, not evidence that upstream Capicola is a sampler. The pinned API exposes host-catalogued files with `uint32` frame count/sample rate, mono or stereo channels, and 8-, 16-, 24-bit PCM or 32-bit float metadata. Its asynchronous reader can convert channel count and bit depth. The wrapper performs sample-rate conversion while reading the loaded buffer.

Accordingly, the wrapper boundary is:

- accept only files that the NT host enumerates through its sample-folder API;
- duplicate mono into Capicola's two processing channels and preserve stereo order;
- load at most the first 1,536,000 frames (32 seconds at the audited 48 kHz baseline) into fixed construction-time memory;
- leave unsupported/unreadable resources to host error behavior; and
- add no recording, looping, reverse, scrubbing, start/end editing, chopping, polyphony, source mixing, or sample substitution.

The SDK header does not promise that every file with a `.wav` suffix is accepted, so this project does not make that broader claim.

## Deliberate exclusions

The first release does not expose Capicola's Alchemy-panel modulation matrix, a dedicated trigger CV, `KeyframeRecorder` record/play transport states, CPU telemetry, or the engine's uncapped TKEO resonance setter. These are implementation details or excluded controls, not an invitation to create adjacent features.

## Reproduction

```sh
git submodule update --init --recursive
make verify-audit
make test-upstream
```

`verify-audit` checks the approved-Spec digest, submodule gitlinks/working commits, SDK API version, unique capability IDs, source locators, limits, analysis signal count, and the explicit sampler exclusions. `test-upstream` compiles the pinned host DSP tests with strict warnings and runs them outside the repository build tree.
