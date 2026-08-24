# Capicola for disting NT — Technical reference

This is the compact maintainer reference for Capicola for disting NT. Players
should start with the [installation and user guide](../README.md).

## Released implementation

The current published binary is
[`v0.5.0`](https://github.com/thorinside/capicola-nt/releases/tag/v0.5.0).

| Item | Value |
| --- | --- |
| Factory name | `Capicola` |
| GUID | `ThCa` |
| Factory tag | Effect |
| Supported firmware baseline | disting NT 1.16.0 |
| Plug-in API | v13 (`kNT_apiVersion13`) |
| Audited audio rate | 48 kHz |
| Installable asset | `capicola.o` |
| Corresponding-source asset | `capicola-nt-source.tar.gz` |

The wrapper is deliberately narrow. It links the pinned Capicola DSP, provides
one stereo engine for mutually exclusive Live and Sample sources, exposes the
audited processing controls, and uses ordinary NT parameter mapping for CV.
Changing source resets the engine so history from the replaced source cannot
sound afterward.

Live mode reads the selected logical buses, with optional left-to-right mono
normalization. Sample mode uses the API v13 folder catalogue and asynchronous
WAV reader. It loads up to 1,536,000 frames from the exact selected catalogue
entry into a fixed stereo float buffer, then loops that buffer forward. Entering
Sample mode and changing Folder both initiate a valid selected load outside the
audio callback. Folder changes also synchronize Sample into the new legal
range through the callback-safe host setter. The source-rate/host-rate ratio
advances a linearly interpolated playback cursor that wraps in constant bounded
work. Both channels render to private scratch buffers before Add/Replace audio
output writes. Four optional analysis signals replace their selected CV buses
and default to `None`.

All persistent DSP, sample, request, and block storage comes from the memory
supplied at construction; the audio path performs no heap allocation. Typed
objects and scratch buffers are explicitly aligned within those byte
allocations. Engine reset is constant-time with respect to the large sparse
rings: it resets their live range and sentinel rather than clearing roughly half
a megabyte of DRAM. The three persistent shaper tables are initialized directly
in that storage; the ARM build rejects any function whose static stack
requirement exceeds 1 KiB. The audio callback also performs no SD catalogue
queries, parameter-definition updates, or file reads. A missing source block or
non-finite DSP result ramps the last valid output to silence; catalogue refresh
and sample recovery wait for a later parameter/UI event.

The custom UI owns three pressable pots and two pressable encoders. Its two
labelled pot banks and temporary folder/sample selector are documented in the
[user guide](../README.md). The title prioritizes the loaded sample name; the
footer contains only Mix, with analysis activity left to the optional CV
outputs. Stretch retains the upstream taper but is rendered as a time factor or
**FREEZE**. All twelve continuous processing controls remain ordinary host
parameters on the Performance page.

## Pinned source and platform

Vendor code is linked through Git submodules rather than copied into the
wrapper history:

| Component | Repository | Commit |
| --- | --- | --- |
| Capicola | `heavylight-industries/capicola` | `f0fb61cfa7111067b4ec1a642d1b16a0910adb3b` |
| distingNT_API | `expertsleepersltd/distingNT_API` | `cd12d876dbe060859828053efab1cbc98c9df251` |

[`SUBMODULES.lock`](../SUBMODULES.lock) is the concise provenance record. The
[capability audit](CAPABILITY_AUDIT.md) traces every exposed processing control
to the pinned upstream implementation and records the NT-specific boundaries.

## Build and verify

An ARM GNU toolchain with `arm-none-eabi-c++`, `readelf`, and `nm` is required.

```sh
git submodule update --init --recursive
make verify
```

`make verify` checks the capability and license ledgers, runs the upstream and
host integration tests with warnings as errors, builds the ARM object, and
verifies that it is an ELF32 little-endian ARM relocatable object exporting
`pluginEntry`. The result is `build/plugins/capicola.o`.

To prepare both public release assets:

```sh
make release-assets
```

This also builds `build/release/capicola-nt-source.tar.gz`. GitHub's automatic
source snapshots omit submodule contents and are not the corresponding-source
download for this project.

## License and releases

The wrapper and incorporated Capicola DSP are released under
`AGPL-3.0-only`. The disting NT API dependency is MIT-licensed. See
[`LICENSE`](../LICENSE), [`NOTICE`](../NOTICE),
[`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md), and the
[source offer](SOURCE_OFFER.md).

After explicit owner approval, pushing a `v*` tag runs the release workflow.
It verifies the tagged commit and publishes `capicola.o` beside the complete
corresponding-source archive.

## Detailed records

These files retain the evidence and decisions that would make the player guide
unnecessarily technical:

- [Capability audit](CAPABILITY_AUDIT.md) — audited controls, ranges, sample
  boundary, and pinned baseline
- [Live routing](LIVE_ROUTING.md) — bus and Add/Replace behavior
- [Sample source](SAMPLE_SOURCE.md) — catalogue, buffered loading, preset, and remount
  lifecycle
- [Host modulation](HOST_MODULATION.md) — parameter-to-CV contract
- [Analysis CV](ANALYSIS_CV.md) — optional output voltages and bus behavior
- [Approved discovery Spec](APPROVED_SPEC.md) and
  [implementation gate](IMPLEMENTATION_GATE.md) — product decisions and scope
- [License audit](LICENSE_AUDIT.md) and [source offer](SOURCE_OFFER.md) — release
  obligations
