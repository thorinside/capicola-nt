# Capicola for disting NT — Technical reference

This is the compact maintainer reference for Capicola for disting NT. Players
should start with the [installation and user guide](../README.md).

## Released implementation

The current published binary is
[`v0.5.2`](https://github.com/thorinside/capicola-nt/releases/tag/v0.5.2).

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
normalization. Sample mode uses the API v13 folder catalogue and streaming API.
Entering Sample mode and changing Folder open the exact valid selection outside
the audio callback. Folder changes also synchronize Sample into the new legal
range through the callback-safe host setter. `NT_streamRender()` converts and
advances the source at the file-rate/host-rate ratio. The opened renderer remains
authoritative if it supplies valid frames beyond the catalogue entry's reported
count, which can differ from a resolved physical variant. Only a short render at
or beyond that reported boundary permits the wrapper's at-most-once-per-block
loop reopen; an earlier short render leaves the remainder silent for that block.
Both channels render to private scratch buffers before Add/Replace audio output
writes. Four optional analysis signals replace their selected CV buses and
default to `None`.

The wrapper treats disting NT audio buses as volts and the pinned Capicola
engine as normalized audio. Live audio is divided by 5 before the engine and
multiplied by 5 at the audio-output boundary. Streamed WAV frames first receive
processing in their native normalized float domain, then receive the API
sample-player's established 8x output scaling after the engine. The appended
**Input Gain** parameter is an attenuation-only
-60–0 dB wrapper trim shared by Live and Sample. Its changes use a bounded
10 ms smoothing state; the first block starts directly at the restored value.
At 0 dB and 0% Mix, the round trip is voltage-transparent. No limiter is added,
and Add mode may raise the final shared-bus level beyond this plug-in's own
contribution.

All persistent DSP, stream, and block storage comes from the memory supplied at
construction; the audio path performs no heap allocation. Two opaque stream
slots use `NT_globals.streamSizeBytes`, and their two host buffers use
`NT_globals.streamBufferSizeBytes`; no full-file buffer is reserved. One slot
remains audible while the other waits for and buffers its first host block. Typed
objects and scratch buffers are explicitly aligned within those byte
allocations. Engine reset is constant-time with respect to the large sparse
rings: it resets their live range and sentinel rather than clearing roughly half
a megabyte of DRAM. The three persistent shaper tables are initialized directly
in that storage; the ARM build rejects any function whose static stack
requirement exceeds 1 KiB. The audio callback performs no SD catalogue queries
or parameter-definition updates. Its only file operation is the bounded host
stream render, plus one possible reopen at a loop boundary. A stream underrun,
missing source block, or non-finite DSP result ramps or drops toward silence;
catalogue refresh and explicit recovery wait for a parameter/UI event.

The custom UI owns three pressable pots and two pressable encoders. Its two
labelled pot banks and temporary folder/sample selector are documented in the
[user guide](../README.md). The title always retains **CAPICOLA** and follows it
with a sample stem limited to 32 characters using a middle ellipsis; repeated
trailing audio extensions are removed. During a prepared replacement, the title
continues naming the audible old stream until the zero-gain handoff. The footer
contains only Mix, with analysis activity left to the optional CV outputs.
Stretch retains the upstream taper but is rendered as a time factor or
**FREEZE**. Pot-originated values are
mirrored immediately for display while the host commits the parameter update.
All twelve upstream continuous processing controls and the wrapper's Input Gain
remain ordinary host parameters on the Performance page. Input Gain is appended
to the underlying parameter array so every released parameter index remains
preset-stable.

Replacing or explicitly reopening a sample prepares a second fixed stream slot.
The old stream remains audible until `NT_streamRender()` returns the replacement's
first frames. The already-warm Capicola processor then receives the old stream
during a 50 ms fade-out, switches to the buffered replacement at the zero-gain
midpoint, and receives the new stream during a 50 ms fade-in. The phases can meet
inside one audio block, so only one midpoint sample is forced to zero and there
is no cold-engine or block-boundary silence gap. The stream state, prefetch block,
counters, and failure-path held stereo values live in the instance, and the
per-sample work is bounded. This transition does not alter the user-facing
**Fade** processing parameter. Selecting the still-active stream again cancels a
pending handoff. Entering Sample mode from another source still resets the engine
at that source boundary.

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
- [Sample source](SAMPLE_SOURCE.md) — catalogue, streaming, preset, and remount
  lifecycle
- [Host modulation](HOST_MODULATION.md) — parameter-to-CV contract
- [Analysis CV](ANALYSIS_CV.md) — optional output voltages and bus behavior
- [Approved discovery Spec](APPROVED_SPEC.md) and
  [implementation gate](IMPLEMENTATION_GATE.md) — product decisions and scope
- [License audit](LICENSE_AUDIT.md) and [source offer](SOURCE_OFFER.md) — release
  obligations
