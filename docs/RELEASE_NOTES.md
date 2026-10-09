# Capicola for disting NT release notes

## v0.6.1 (candidate)

Simplifies the upstream v1.0 controls by reusing **Pitch** and **Stretch** in
their original parameter slots 9 and 10. Both now range from -100% to +100%
with fresh Pitch at +50% / unity and Stretch at 0% / **Freeze**. Pitch center
remains **Hold**; performance and host Pitch displays show signed multipliers.
Pitch spans signed rates -2…+2; Stretch spans -1…+1 with the signed magnitude
taper `sign(x) * abs(x)^2.5`. Forward unity is Pitch +50%, Stretch +100%.

- Removes the two appended v0.6.0 bipolar controls. All 26 pre-v0.6.0 parameter
  indices and page positions remain; the other 24 definitions are unchanged.
- Defaults unversioned presets to original format 1, including presets with no
  custom data. Every save carries a version tag matching its stored values.
  Old Pitch -12…+12 semitones maps linearly to -100…+100%; old Stretch 0…100%
  maps linearly to -100…+100%. Old center Pitch becomes Hold, while old Stretch
  0/50/100 becomes reverse/freeze/forward. Converted presets use the new sound
  and CV response.
- Converts old values once before the first audio block. Fresh instances start
  at unity Pitch and Freeze Stretch; saved format 2 presets and later edits use
  native bipolar values. Saving before the first audio block also reloads correctly.
- Retains the upstream v1.0 DSP fixes and event-based transient outputs from
  v0.6.0. v0.6.0 preset compatibility is explicitly outside this update's scope;
  its published tag and assets remain intact.

Validation: strict host DSP and factory integration suites, both restoration
orders before operation, original presets with no custom-data callback, fresh
defaults, one-time range conversion, mapped CV and save/reload before and after
the first audio block, capability/license audits, and an inspected Cortex-M7
ARM object.
Firmware 1.16.0, API v13 and 48 kHz remain the baseline. Physical NT preset loading
and listening have not been performed. The technical reference distinguishes the
owner-supplied inactive-load lifecycle from the SDK's callback permissions.

## v0.6.0

Updates the portable Capicola DSP to upstream **v1.0.0**, commit
`120b0b843c18bda373d96eb79d49805545f61556`.

- **Reverse processing:** Adds Bipolar Pitch and Bipolar Stretch to the end of
  the Performance page and parameter array. Both default to 0%, which preserves
  the legacy rate. -50% stops the pitch head or freezes the stretch grid;
  -100% reaches full reverse (-2× pitch, -1× stretch), and +100% reaches the
  forward limit. The SD-file stream remains forward; these controls act on the
  captured DSP history in both Live and Sample modes.
- **Upstream fixes:** Includes reverse window seeking, backward crossfades,
  buffer-edge re-anchoring and crossfaded pitch direction changes.
- **Transient outputs:** Uses upstream's accepted slice and follower events
  instead of its removed gate API. Assigned outputs produce non-retriggerable
  10 ms, 5 V pulses. Natural detectors have a 150 ms holdoff; valid manual Slice
  also produces an input pulse. Startup, pitch flips and stereo guard requests
  do not add musical trigger events.
- **Preset compatibility:** Retains all 26 original parameter definitions,
  their indices and every existing page entry. Legacy Pitch, Stretch and
  Quality tapers remain intact; the new controls occupy indices 26 and 27.
  There is no additional mode or preset format.

Validation covers the upstream host suite both unmodified and with the four NT
portability overlays, wrapper DSP and exported-host integration tests, literal
old preset vectors with defaulted appended values, mapped changes in both
sources, pulse timing, and an inspected Cortex-M7 ARM object. Physical NT preset
loading and listening were not performed for this release. Firmware 1.16.0,
API v13 and 48 kHz remain the supported baseline.

## v0.5.3

This release fixes six wrapper behaviors while retaining the pinned Capicola
DSP, parameter indices, control ranges, and firmware 1.16.0 / API v13 / 48 kHz
baseline.

- **Sample recovery:** A stream that has already played can recover after
  100 ms without further frames, including a physical variant shorter than its
  catalogue entry. Initial loading waits for first progress without repeatedly
  restarting. Reopens remain limited to one per audio block.
- **Slice during startup:** Pressing Slice before the first processed block
  is ignored and can no longer cancel engine startup or leave audio permanently
  unprocessed.
- **Live catalogue changes:** Changing an inactive Folder or refreshing SD
  mount state preserves the Live engine and its audio history.
- **Source switching:** A 10 ms linear bridge carries the last audible stereo
  voltage into the new source output while the engine resets. It accounts for
  the different Live and Sample output gains. The initial Sample fade waits for
  real audio before starting its 50 ms ramp. Sample-replacement fades retain
  their 50 ms fade-out and 50 ms fade-in.
- **Stereo timing:** Upstream coordination is restored: when one channel
  automatically catches up and the source-grid lags differ by more than one
  second, the other channel also receives a Slice.
- **Pot banks:** After switching banks, pots must reach or pass their displayed
  values before changing them. A bank press alone cannot change a control.

The streaming API reports frame counts without an EOF indicator. A shorter
physical variant can have a 100 ms gap before looping, and an underrun lasting
at least 100 ms may restart playback. Seamless looping remains unguaranteed;
this release adds no loop-point editing or loop-boundary crossfade.

See the [player guide](https://github.com/thorinside/capicola-nt/blob/v0.5.3/README.md),
[sample-source details](https://github.com/thorinside/capicola-nt/blob/v0.5.3/docs/SAMPLE_SOURCE.md),
and [technical reference](https://github.com/thorinside/capicola-nt/blob/v0.5.3/docs/TECHNICAL.md)
for the complete behavior.
