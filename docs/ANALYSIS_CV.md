# Analysis CV routing

Capicola exposes the four analysis signals confirmed by the pinned capability audit as optional NT CV outputs:

| Selector | Signal | Voltage |
| --- | --- | --- |
| `Input Transient output` | Accepted automatic slice from either recorder, or valid manual Slice | 10 ms pulse, 0 V low, 5 V high |
| `Output Transient output` | Kept transient from the post-mix output detector | 10 ms pulse, 0 V low, 5 V high |
| `Input Envelope output` | Maximum normalized envelope of the two input-channel detectors | 0–5 V |
| `Output Envelope output` | Normalized post-mix output envelope | 0–5 V |

All four selectors are ordinary NT CV-output parameters on the **Routing** page. Their minimum, first-load default, and host-reset default are `0` (`None`). At `0`, Capicola does not read, write, or claim a bus. Select any available NT bus to assign a signal, and select `0` again to disconnect it.

An assigned analysis signal replaces the selected bus with its voltage. Assigning multiple outputs, including Capicola's audio outputs, to the same bus is therefore not a mixing facility and should be avoided. These selectors do not add processing-control CV inputs; ordinary parameter modulation remains host-owned as described in [HOST_MODULATION.md](HOST_MODULATION.md).

The envelope normalization mirrors the upstream analysis path. Input analysis
combines the linked channel detectors; output analysis observes the post-mix
stereo sum. Both Live and Sample use these same signals.

Upstream v1.0 replaced detector gates with event counts. The NT adapter emits a
480-frame pulse at the pinned 48 kHz rate. A pulse ignores further events while
high and never queues a delayed retrigger. Input events are quantized to the
start of their processed block or sample-handoff segment; output follower
events retain their sample position. A stereo pair of automatic slices counts
as one event. Startup seeding, pitch direction flips and the stereo catch-up
guard create no input event. Manual Slice is ignored before processing starts.
Natural kept events have the upstream 150 ms per-detector holdoff, and Fade can
further limit automatic slices; manual Slice bypasses those musical holdoffs.

Pulse timers advance even when their bus is disconnected. Missing/invalid
source segments write zero and age the timers; a cold source reset clears them.
Warm sample replacement preserves the pulse state across its two segments.
