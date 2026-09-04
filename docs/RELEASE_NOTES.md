# Capicola for disting NT release notes

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
