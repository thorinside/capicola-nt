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
| Input Gain | 0 dB | Attenuation-only -60–0 dB trim before dry/wet processing. |

Each connected input is processed by its matching independent Capicola channel. There is no summing or channel swap. With Right input set to `0`, the Left input is sent to both processors. Selecting a Right input bus restores independent stereo processing; it does not mix that bus with Left.

The wrapper converts the NT's ±5 V audio convention to Capicola's normalized
audio domain before processing and converts back at the output. With Input Gain
at 0 dB and Mix at 0%, output equals input sample-for-sample and volt-for-volt.
Lower Input Gain when the wet path is being driven too hard. Replace preserves
this plug-in contribution on a dedicated bus; Add can produce a louder final
bus by summing signal that was already present.

Both channels are rendered to private DRAM scratch space before either output bus is written, so the routing remains deterministic even when selected input and output buses alias. The path uses the pinned upstream `KeyframeRecorder` in `LIVE_EFFECT` state and performs no allocation, file access, or logging in the audio callback.

This document covers the live-audio path only. Source-mode selection, processing controls, the custom performance screen, sample playback, and analysis CV outputs belong to later delivery slices.
