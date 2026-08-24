# Live and loaded-sample sources

This delivery uses the disting NT firmware/SDK **1.16.0** sample catalogue and stream APIs. It does not scan arbitrary files itself.

## Selecting a source

The `Source` parameter has two mutually exclusive values:

- **Live** processes `Left input` and `Right input`. A disconnected right input (`0`) duplicates the left input into both Capicola channels.
- **Sample** replaces both live inputs with the selected host-catalogued sample. Live audio is not read or mixed while this mode is active.

Changing source resets Capicola's source history. This prevents audio retained from the previous source from sounding after the replacement. During ordinary interaction, entering Sample mode does not implicitly open a file: confirm `Sample` to open the displayed valid selection. Preset restoration and SD-card remount follow the pinned SDK host lifecycle described below. Sample mode stays selected if the card or file is unavailable and supplies silence to Capicola; it never falls back to live input or chooses another sample.

## Loading a sample

1. Put a sample in a location enumerated by disting NT's sample-folder catalogue.
2. Turn the left encoder on the persistent Capicola performance screen to select **Sample** (or set the `Source` parameter).
3. Press the left encoder to temporarily replace the performance screen with **SELECT FOLDER**. Turn to choose a host-catalogued folder, then press to continue.
4. On **SELECT SAMPLE**, turn to choose the displayed host-catalogued sample and press to load it. The persistent performance screen returns immediately and shows **SAMPLE PLAY** after a successful open or **SAMPLE WAIT** when no stream is open.

The folder and sample names shown by the parameters come from the host catalogue. Changing `Folder` updates the legal `Sample` range and closes the prior stream, but does not open a file; confirming `Sample` opens that exact valid catalogue entry as a one-shot stream from its beginning. Confirming it again reopens it. An invalid folder or sample value is not clamped to a different catalogue entry, displays no substituted name, and leaves Sample mode selected but silent. The wrapper requests normal sequential, forward playback and uses the file sample rate to ask the host streamer for rate conversion to the 48 kHz baseline.

## Presets, remounts, and unavailable samples

`Source`, `Folder`, and `Sample` are ordinary NT parameters, so the host stores and restores them with the rest of a preset; Capicola adds no separate file database or custom preset format. On a fresh Sample-mode instance, the restored values may arrive before the SD catalogue is mounted. When the catalogue becomes available, the wrapper follows the pinned SDK sample-streamer lifecycle and reapplies the saved `Sample` selection. A valid reference therefore reopens from the beginning. The same host lifecycle reapplies the current selection after an SD-card remount.

This behavior is grounded in distingNT_API commit `cd12d876dbe060859828053efab1cbc98c9df251`, where `examples/sampleStreamer.cpp` updates the folder range and calls its sample parameter handler when `NT_isSdCardMounted()` changes to mounted. The API exposes catalogue indices and metadata through `NT_getNumSampleFolders()`, `NT_getSampleFolderInfo()`, and `NT_getSampleFileInfo()`, and makes `NT_streamOpen()` success authoritative.

If the saved folder/sample catalogue entry is missing or moved so the saved indices are no longer valid, the wrapper performs no out-of-range lookup and opens nothing. If metadata is unsupported (including a zero sample rate), it does not call the streamer. If `NT_streamOpen()` rejects an unreadable resource, the failed open is retained. In every case, `Source` remains **Sample**, Capicola's prior source history is cleared, and the outputs are silent; the wrapper neither selects another catalogue entry nor falls back to **Live**. The host owns catalogue construction and error presentation, so the wrapper does not add a recovery dialog or file substitution policy.

Mono files are delivered by the host stream as stereo and therefore feed identical left and right Capicola channels. Stereo files retain left/right order. The wrapper adds no duration limit, loop, reverse, scrub, region, chopping, polyphony, recording, or live/sample mix feature.

Supported catalogue metadata and format boundaries are recorded in [`CAPABILITY_AUDIT.md`](CAPABILITY_AUDIT.md). Unsupported, unreadable, missing, or unmounted resources retain host behavior.

## Performance controls

The custom performance screen keeps the active **LIVE**/**SAMPLE PLAY**/**SAMPLE WAIT** source state, three immediate controls, MAIN/ALT state, Mix, and Capicola input/output activity visible. **IN** and **OUT** show the normalized envelope level from 00–99; `!` beside either value reports its transient detector. It follows the approved v2 interaction hierarchy:

- Main pots: **Stretch**, **Threshold**, **Feedback**.
- Press any of the three pots to switch the complete bank to the clearly labelled alternate trio: **Pitch**, **Grain Size**, **Quality**. The active MAIN/ALT identity, each active control name, and each value remain visible.
- Turn the right encoder for **Mix**; press it for the audited momentary **Slice** action.
- Turn the left encoder to replace Live with Sample or Sample with Live. In Sample mode, press it to enter the temporary folder/sample selection described above.
- The temporary selector uses only the left encoder. Pressing after a sample choice loads it and returns to the one persistent performance screen; pots and the right encoder do not change performance controls while selection is open.

No separate module buttons are claimed by the custom UI. All twelve continuous controls are ordinary NT parameters on the **Performance** page, so host parameter-to-CV mapping remains available. The persistent screen keeps the approved v2 immediate/alternate hierarchy above; the host parameter view exposes the five secondary controls without adding a routine performance page or materially changing that screen:

- **Envelope Smoothing:** 0.00–100.00% normalized sweep maps exponentially to cutoff 0.00005–0.125 (about 1.2 Hz–3 kHz at 48 kHz); default 42.59% (about 0.0014).
- **Fade:** 0.00–100.00% maps exponentially to 10–250 ms (480–12000 frames at 48 kHz); default 21.53% (about 20 ms).
- **Drive:** 0.00–100.00% maps linearly to 0.5–4.0; default 14.29% (about 1.0).
- **Drive Character:** 0.00% quake, 50.00% clean, 100.00% sinc; default 100.00%.
- **Feedback Tone:** 0.00–100.00% maps exponentially to normalized bandpass center 0.002–0.9 (about 48 Hz–21.6 kHz at 48 kHz); default 37.69% (about 0.02).

These controls use the exact audited upstream sweep equations, are shared by the left and right Capicola channels (Feedback Tone controls both feedback filters), and affect Live and Sample through the same processing path. The existing controls retain their audited ranges and tapers: Pitch ±12 semitones, Stretch realtime-to-freeze `(1-x)^2.5`, Threshold ratio 0–8/top=mute, Grain Size 32–4096 keyframes, Quality ε 0.1–0.001, Feedback 0–1.5, and Mix dry-to-wet. Together with the right-encoder **Slice** action, the interface represents all 13 audited processing capabilities.

## Processing and routing

Both source modes enter the same stereo-linked Capicola processing path and use the same confirmed controls, audio outputs, and Add/Replace output modes. Source replacement resets the engine, after which the current processing parameter values are reapplied. This slice does not change the audited processing-capability set or claim that loaded-sample playback is an upstream Capicola sampler feature.
