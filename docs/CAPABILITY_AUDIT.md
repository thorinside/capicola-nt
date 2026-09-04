# Audited Capicola capability set

This is the controlling capability boundary for the first disting NT release. The machine-readable ledger is [`capability-set.json`](capability-set.json). Future implementation may constrain a listed capability further when measured NT feasibility requires it, but it must not add a processing or sampler capability without a new upstream audit and the review required by the approved Spec.

## Pinned evidence baseline

| Component | Pinned revision | Meaning |
|---|---|---|
| Capicola | `f0fb61cfa7111067b4ec1a642d1b16a0910adb3b` (`main`, no upstream release tag) | Audited DSP, panel labels/ranges, and analysis signals |
| distingNT_API | tag `v1.16.0`, commit `cd12d876dbe060859828053efab1cbc98c9df251` | Delivery SDK baseline |
| disting NT firmware | exactly `1.16.0` | Delivery/runtime baseline; no other firmware is implied supported |
| Plug-in API | `kNT_apiVersion13` | Custom UI, sample streaming, 64-bus constants, and dynamic parameter-page support |
| Audio rate | 48 kHz | Capicola constants and all Hz/ms conversions in this audit |

Both repositories are Git submodules under `vendor/`; the gitlinks, not branch names, are authoritative. The SDK's `v1.15.0` and `v1.16.0` tags currently point to the same commit, so the exact commit is recorded to remove tag ambiguity. Firmware 1.16.0 still requires later ARM, emulator, and physical-runtime verification before release; this audit does not claim those later acceptance checks.

## First-release processing surface

The included user-facing processing capabilities are:

- **Pitch:** -12 to +12 semitones. The NT control uses 0.1-semitone steps with zero directly selectable; it does not apply the upstream panel's continuous zero-detent snap.
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
- **Slice:** momentary manual splice action, ignored until the engine has started processing.

Every control is stereo-linked, and ordinary modulation uses NT parameter mapping. The JSON ledger records a source symbol and an NT-specific constraint for every item.

The wrapper retains the upstream stereo coordination rule: if exactly one
channel automatically fires in a block and the source-grid lags differ by more
than 48,000 frames (one second at 48 kHz), the other channel receives a Slice.
The two detectors remain independent below this threshold. Switching the
custom UI's pot bank requires each pot to cross its current displayed value or
come within 0.005 on its normalized sweep; initial screen entry retains host
takeover.

## Audited analysis outputs

Four upstream signals are eligible for optional output assignment: **Input Transient**, **Output Transient**, **Input Envelope**, and **Output Envelope**. Transients are 0/5 V and envelopes are 0–5 V. Each future selector must include and default to `0`/disconnected. The output transient is analysis-only and never splices the engine.

This count fits the pinned platform's eight physical output buses, but allocation remains optional and must coexist with the two audio outputs. No bus is claimed by default.

## Sample-source boundary

Streamed-sample mode is an approved NT wrapper source adapter, not evidence that upstream Capicola is a sampler. The pinned API exposes host-catalogued files with `uint32` frame count/sample rate, mono or stereo channels, and 8-, 16-, 24-bit PCM or 32-bit float metadata. Its streaming API supplies stereo float frames at a caller-selected file-frame/host-frame rate.

Accordingly, the wrapper boundary is:

- accept only files that the NT host enumerates through its sample-folder API;
- duplicate mono into Capicola's two processing channels and preserve stereo order;
- reserve two API-sized per-instance stream states and buffers plus two host blocks of rendered frames rather than a full-file buffer;
- open the current valid selection when Sample mode is entered or Folder changes in Sample mode, synchronize Sample to a changed folder's legal range, and loop the full stream because this source has no transport trigger; inactive Folder changes and SD mount-state refreshes leave the Live engine intact;
- retain the refreshed catalogue metadata for validation while treating valid frames from the opened renderer as authoritative, so a longer resolved physical variant is not restarted at the catalogue entry's earlier reported boundary;
- preserve the warm processor across a valid sample-to-sample replacement, keep the old stream audible until the pending stream returns frames, switch streams at the zero-gain midpoint of a fixed 50 ms fade-out/50 ms fade-in, and leave the exposed Fade processing control unchanged;
- reset the engine on a Live/Sample source switch and linearly bridge its last audible stereo voltage into the new output over 10 ms, retaining the initial Sample 50 ms fade-in;
- after first stream progress, permit a loop reopen on a short render at or beyond the reported boundary, or recover an earlier stall after 100 ms of missing host frames; reset the missing-frame count on any progress, reopen, or handoff, and never reopen initial loading before its first progress;
- bound recovery to one reopen per audio block and leave unavailable frames silent rather than performing unbounded SD work;
- leave unsupported/unreadable resources to host error behavior; and
- add no recording, reverse, scrubbing, start/end editing, chopping, polyphony, source mixing, hidden file substitution, loop-point editing, or loop-boundary crossfade.

The SDK header does not promise that every file with a `.wav` suffix is accepted, so this project does not make that broader claim.

API v13 supplies no separate EOF status. A shorter physical variant can
therefore have a 100 ms gap before restarting, and an underrun lasting at least
100 ms can restart playback. The recovery policy does not guarantee seamless
EOF or loop boundaries.

## Deliberate exclusions

The first release does not expose Capicola's Alchemy-panel modulation matrix, a dedicated trigger CV, `KeyframeRecorder` record/play transport states, CPU telemetry, or the engine's uncapped TKEO resonance setter. These are implementation details or excluded controls, not an invitation to create adjacent features.

## Reproduction

```sh
git submodule update --init --recursive
make verify-audit
make test-upstream
```

`verify-audit` checks the approved-Spec digest, submodule gitlinks/working commits, SDK API version, unique capability IDs, source locators, limits, analysis signal count, and the explicit sampler exclusions. `test-upstream` compiles the pinned host DSP tests with strict warnings and runs them outside the repository build tree.
