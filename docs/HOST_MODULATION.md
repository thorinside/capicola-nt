# Host parameter-to-CV modulation

Capicola uses disting NT's ordinary parameter mapping for non-audio modulation. It does not add dedicated Pitch CV, Stretch CV, or other processing-control input parameters.

## Mappable controls

All twelve continuous upstream processing controls plus the wrapper gain trim
are ordinary NT parameters on the **Performance** page:

- Input Gain
- Pitch
- Stretch
- Threshold
- Grain Size
- Quality
- Feedback
- Envelope Smoothing
- Fade
- Drive
- Drive Character
- Mix
- Feedback Tone

Choose one of these parameters in the disting NT host's parameter mapping interface and assign the desired CV source there. The host owns the mapping, including its base value and effective mapped value. The wrapper consumes that effective value on every audio callback, so mapped changes reach the same stereo-linked Capicola control path used by the performance UI in both Live and Sample modes.

Input Gain is constrained to -60–0 dB and therefore cannot add gain. It is
smoothed in the wrapper and affects both dry and wet audio before the Mix
crossfade; it does not change the pinned Capicola DSP.

The only audio input parameters claimed by the wrapper are **Left input** and **Right input**. Those select the Live source buses; they are not dedicated modulation inputs. A disconnected Right input retains the documented left-to-both-channel normalization.

Parameter ranges and tapers are listed in [CAPABILITY_AUDIT.md](CAPABILITY_AUDIT.md) and [SAMPLE_SOURCE.md](SAMPLE_SOURCE.md). Host mapping does not expand those audited ranges or expose Capicola's excluded Alchemy modulation matrix.

## Verification boundary

The API v13 host contract supplies each parameter's effective value, including mapping, in the algorithm parameter array. The wrapper reapplies the continuous processing values from that array before each block. The host integration test changes Mix's effective value between callbacks without invoking the custom UI or the parameter-change callback, then proves that Sample processing changes from dry to wet. It also verifies 0 dB/0% Mix voltage transparency, -12 dB attenuation, the normalized wet-DSP boundary, and that the two Live audio inputs are the plugin's only audio-input parameters.
