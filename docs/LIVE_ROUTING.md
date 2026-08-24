# Live stereo routing

This delivery implements Capicola's live-effect path on the pinned disting NT firmware 1.16.0 / API v13 baseline.

## Parameters

| Parameter | Default | Behavior |
|---|---:|---|
| Left input | 1 | Required live left source. |
| Right input | 2 | Live right source. Select `0`/None to normalize the left source to both Capicola channels. |
| Left output | 13 | Processed left destination. |
| Left output mode | Add | Standard NT Add/Replace behavior. |
| Right output | 14 | Processed right destination. |
| Right output mode | Add | Standard NT Add/Replace behavior. |

Each connected input is processed by its matching independent Capicola channel. There is no summing or channel swap. With Right input set to `0`, the Left input is sent to both processors. Selecting a Right input bus restores independent stereo processing; it does not mix that bus with Left.

Both channels are rendered to private DRAM scratch space before either output bus is written, so the routing remains deterministic even when selected input and output buses alias. The path uses the pinned upstream `KeyframeRecorder` in `LIVE_EFFECT` state and performs no allocation, file access, or logging in the audio callback.

This document covers the live-audio path only. Source-mode selection, processing controls, the custom performance screen, sample playback, and analysis CV outputs belong to later delivery slices.
