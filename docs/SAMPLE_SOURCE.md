# Live and loaded-sample sources

This delivery uses the disting NT firmware/SDK **1.16.0** sample catalogue and stream APIs. It does not scan arbitrary files itself.

## Selecting a source

The `Source` parameter has two mutually exclusive values:

- **Live** processes `Left input` and `Right input`. A disconnected right input (`0`) duplicates the left input into both Capicola channels.
- **Sample** replaces both live inputs with the selected host-catalogued sample. Live audio is not read or mixed while this mode is active.

Changing source resets Capicola's source history. This prevents audio retained from the previous source from sounding after the replacement. Entering Sample mode does not implicitly open a file: confirm `Sample` to open the displayed valid selection. Sample mode stays selected if the card or file is unavailable and supplies silence to Capicola; it never falls back to live input or chooses another sample.

## Loading a sample

1. Put a sample in a location enumerated by disting NT's sample-folder catalogue.
2. Turn the left encoder on the persistent Capicola performance screen to select **Sample** (or set the `Source` parameter).
3. Use the host's temporary `Source` parameter view to choose `Folder` and the displayed `Sample`.
4. Press the left encoder on the performance screen, or confirm `Sample` in the host view, to open that exact selection.

The folder and sample names shown by the parameters come from the host catalogue. Changing `Folder` updates the legal `Sample` range and closes the prior stream, but does not open a file; confirming `Sample` opens that exact valid catalogue entry as a one-shot stream from its beginning. Confirming it again reopens it. Entering Sample mode or remounting the card also leaves the source silent until `Sample` is confirmed. An invalid folder or sample value is not clamped to a different catalogue entry, displays no substituted name, and leaves Sample mode selected but silent. The wrapper requests normal sequential, forward playback and uses the file sample rate to ask the host streamer for rate conversion to the 48 kHz baseline.

Mono files are delivered by the host stream as stereo and therefore feed identical left and right Capicola channels. Stereo files retain left/right order. The wrapper adds no duration limit, loop, reverse, scrub, region, chopping, polyphony, recording, or live/sample mix feature.

Supported catalogue metadata and format boundaries are recorded in [`CAPABILITY_AUDIT.md`](CAPABILITY_AUDIT.md). Unsupported, unreadable, missing, or unmounted resources retain host behavior.

## Performance controls

The custom performance screen keeps the active **LIVE**/**SAMPLE** source, three immediate controls, MAIN/ALT state, and Mix visible. It follows the approved v2 interaction hierarchy:

- Main pots: **Stretch**, **Threshold**, **Feedback**.
- Press any pot to expose the clearly labelled alternate trio: **Pitch**, **Grain Size**, **Quality**.
- Turn the right encoder for **Mix**; press it for the audited momentary **Slice** action.
- Turn the left encoder to replace Live with Sample or Sample with Live; in Sample mode, press it to confirm the displayed host-catalogued sample.

All seven continuous controls are ordinary NT parameters as well as custom-screen controls, so host parameter-to-CV mapping remains available. Their ranges and tapers are the audited upstream ranges in [`CAPABILITY_AUDIT.md`](CAPABILITY_AUDIT.md): Pitch ±12 semitones, Stretch realtime-to-freeze `(1-x)^2.5`, Threshold ratio 0–8/top=mute, Grain Size 32–4096 keyframes, Quality ε 0.1–0.001, Feedback 0–1.5, and Mix dry-to-wet.

## Processing and routing

Both source modes enter the same stereo-linked Capicola processing path and use the same confirmed controls, audio outputs, and Add/Replace output modes. Source replacement resets the engine, after which the current processing parameter values are reapplied. This slice does not change the audited processing-capability set or claim that loaded-sample playback is an upstream Capicola sampler feature.
