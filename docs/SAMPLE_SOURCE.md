# Live and loaded-sample sources

This delivery uses the disting NT firmware/SDK **1.16.0** sample catalogue and asynchronous WAV reader. It does not scan arbitrary files itself.

## Selecting a source

The `Source` parameter has two mutually exclusive values:

- **Live** processes `Left input` and `Right input`. A disconnected right input (`0`) duplicates the left input into both Capicola channels.
- **Sample** replaces both live inputs with the selected host-catalogued sample. Live audio is not read or mixed while this mode is active.

Changing source resets Capicola's source history. This prevents audio retained from the previous source from sounding after the replacement. Entering Sample mode immediately loads the displayed valid folder/sample selection. Sample mode stays selected if the card or file is unavailable and supplies silence to Capicola; it never falls back to live input.

## Loading a sample

1. Put a sample in a location enumerated by disting NT's sample-folder catalogue.
2. Turn the left encoder on the persistent Capicola performance screen to select **Sample** (or set the `Source` parameter). The current valid selection begins loading immediately.
3. Press the left encoder to temporarily replace the performance screen with **SELECT FOLDER**. Turn to choose a host-catalogued folder, then press to continue.
4. On **SELECT SAMPLE**, turn to choose the displayed host-catalogued sample and press to load it. The persistent performance screen returns immediately, shows **LOADING** with the selected name during the asynchronous read, then uses the playing sample's name as its title. **SAMPLE WAIT** means no sample is currently available.

The folder and sample names shown by the Source page and custom selector come from the host catalogue. `Folder` uses the host string-picker contract and `Sample` uses the host confirm-picker contract, so compatible controllers can present the folder and sample browsers. Changing `Folder` updates the legal `Sample` range and immediately loads the selected valid entry in that folder. If the old Sample number is outside the new folder's range, Capicola updates the visible host parameter to the nearest legal value before loading; recursive host callbacks are guarded. Pressing **LOAD** in Capicola's sample selector deliberately reloads that exact valid catalogue entry into a fixed stereo float buffer and restarts playback.

The wrapper requests stereo 32-bit float frames, allowing the host reader to duplicate mono or convert supported PCM formats. It reads at most the first **1,536,000 frames**: 32 seconds at 48 kHz, 64 seconds at 24 kHz, or 16 seconds at 96 kHz. Playback loops that loaded range, uses linear interpolation, and advances by the file-sample-rate/host-sample-rate ratio. It applies the NT sample-player level scaling before both channels enter Capicola. There is no transport trigger input; the loop runs while Sample mode remains active. Files intended to loop seamlessly must have a suitable boundary because the wrapper adds no loop-point editor or boundary crossfade.

## Presets, remounts, and unavailable samples

`Source`, `Folder`, and `Sample` are ordinary NT parameters, so the host stores and restores them with the rest of a preset; Capicola adds no separate file database or custom preset format. On a fresh Sample-mode instance, the host's restored parameter callbacks reopen a valid saved selection exactly once, even when the host notifies all three parameters. If restoration happens before its file is available, the screen remains at **SAMPLE WAIT**; reopen the selector and press **LOAD** after the catalogue is ready.

For real-time safety, the audio callback never scans the catalogue, changes parameter definitions, or reads the SD card. It only reads and wraps the fixed memory buffer and runs Capicola. Removing the card after a successful load does not interrupt the loop. Inserting or remounting the card does not automatically recover a failed load from the audio callback; press **LOAD** in the sample selector or change Folder to refresh the catalogue and load again.

The API exposes catalogue indices and metadata through `NT_getNumSampleFolders()`, `NT_getSampleFolderInfo()`, and `NT_getSampleFileInfo()`. Capicola invokes those operations and starts `NT_readSampleFrames()` only during construction or a relevant parameter/user-interface event, not while producing an audio block. A persistent request object owns the asynchronous operation. Its callback makes the buffer playable only after a successful read; a newer source or selection generation rejects an obsolete completion.

If the saved Folder catalogue entry is missing or moved so its index is no longer valid, the wrapper performs no out-of-range lookup and reads nothing. A valid folder synchronizes Sample into that folder's visible legal range. If metadata is unsupported (including a zero sample rate), it does not start a read. If the host rejects or fails an unreadable resource, the buffer does not become playable. In every failure case, `Source` remains **Sample**, Capicola's prior source history is cleared, and the outputs are silent; the wrapper never falls back to **Live**. The host owns catalogue construction and error presentation, so the wrapper does not add a recovery dialog or hidden file substitution policy.

Mono files are delivered by the host reader as stereo and therefore feed identical left and right Capicola channels. Stereo files retain left/right order. The wrapper adds no reverse, scrub, region, chopping, polyphony, recording, or live/sample mix feature.

Supported catalogue metadata and format boundaries are recorded in [`CAPABILITY_AUDIT.md`](CAPABILITY_AUDIT.md). Unsupported, unreadable, missing, or unmounted resources retain host behavior.

## Performance controls

The custom performance screen shows **CAPICOLA LIVE**, **LOADING** with the selected name, the playing sample's name, or **SAMPLE WAIT** in its title. It keeps three immediate controls and Mix visible without the redundant MAIN/ALT or input/output activity numbers. It follows the approved interaction hierarchy:

- Main pots: **Stretch**, **Threshold**, **Feedback**.
- Press any of the three pots to switch the complete bank to the clearly labelled alternate trio: **Pitch**, **Grain Size**, **Quality**. The three active control names and values identify the bank without a separate MAIN/ALT label.
- Turn the right encoder for **Mix**; press it for the audited momentary **Slice** action.
- Turn the left encoder to replace Live with Sample or Sample with Live. In Sample mode, press it to enter the temporary folder/sample selection described above.
- The temporary selector uses only the left encoder. Pressing after a sample choice loads it and returns to the one persistent performance screen; pots and the right encoder do not change performance controls while selection is open.

No separate module buttons are claimed by the custom UI. All twelve continuous controls are ordinary NT parameters on the **Performance** page, so host parameter-to-CV mapping remains available. The persistent screen keeps the approved v2 immediate/alternate hierarchy above; the host parameter view exposes the five secondary controls without adding a routine performance page or materially changing that screen:

- **Envelope Smoothing:** 0.00–100.00% normalized sweep maps exponentially to cutoff 0.00005–0.125 (about 1.2 Hz–3 kHz at 48 kHz); default 42.59% (about 0.0014).
- **Fade:** 0.00–100.00% maps exponentially to 10–250 ms (480–12000 frames at 48 kHz); default 21.53% (about 20 ms).
- **Drive:** 0.00–100.00% maps linearly to 0.5–4.0; default 14.29% (about 1.0).
- **Drive Character:** 0.00% quake, 50.00% clean, 100.00% sinc; default 100.00%.
- **Feedback Tone:** 0.00–100.00% maps exponentially to normalized bandpass center 0.002–0.9 (about 48 Hz–21.6 kHz at 48 kHz); default 37.69% (about 0.02).

These controls use the exact audited upstream sweep equations, are shared by the left and right Capicola channels (Feedback Tone controls both feedback filters), and affect Live and Sample through the same processing path. The existing controls retain their audited ranges and tapers: Pitch ±12 semitones, Stretch realtime-to-freeze `(1-x)^2.5`, Threshold ratio 0–8/top=mute, Grain Size 32–4096 keyframes, Quality ε 0.1–0.001, Feedback 0–1.5, and Mix dry-to-wet. The Stretch value is displayed as its time factor (about 5.7× at midpoint) and **FREEZE** at the clockwise stop. For an uninterrupted long stretch, use 100% Mix and 100% Threshold: lower Threshold values intentionally accept transients that catch the read head up to the current source. A larger Grain Size increases the distance between adaptive splices but does not extend the maximum beyond freeze. Together with the right-encoder **Slice** action, the interface represents all 13 audited processing capabilities.

## Processing and routing

Both source modes enter the same stereo-linked Capicola processing path and use the same confirmed controls, audio outputs, and Add/Replace output modes. Source replacement resets the engine, after which the current processing parameter values are reapplied. This slice does not change the audited processing-capability set or claim that loaded-sample playback is an upstream Capicola sampler feature.
