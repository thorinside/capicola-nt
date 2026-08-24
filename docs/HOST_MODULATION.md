# Host parameter-to-CV modulation

Capicola uses disting NT's ordinary parameter mapping for non-audio modulation. It does not add dedicated Pitch CV, Stretch CV, or other processing-control input parameters.

## Mappable controls

All twelve continuous processing controls are ordinary NT parameters on the **Performance** page:

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

The only audio input parameters claimed by the wrapper are **Left input** and **Right input**. Those select the Live source buses; they are not dedicated modulation inputs. A disconnected Right input retains the documented left-to-both-channel normalization.

Parameter ranges and tapers are listed in [CAPABILITY_AUDIT.md](CAPABILITY_AUDIT.md) and [SAMPLE_SOURCE.md](SAMPLE_SOURCE.md). Host mapping does not expand those audited ranges or expose Capicola's excluded Alchemy modulation matrix.

## Verification boundary

The API v13 host contract supplies each parameter's effective value, including mapping, in the algorithm parameter array. The wrapper reapplies the continuous processing values from that array before each block. The host integration test changes Mix's effective value between callbacks without invoking the custom UI or the parameter-change callback, then proves that Sample processing changes from dry to wet. It also verifies that the two Live audio inputs are the plugin's only audio-input parameters.
