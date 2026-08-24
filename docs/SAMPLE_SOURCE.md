# Live and loaded-sample sources

This delivery uses the disting NT firmware/SDK **1.16.0** sample catalogue and asynchronous WAV reader. It does not scan arbitrary files itself.

## Selecting a source

The `Source` parameter has two mutually exclusive values:

- **Live** processes `Left input` and `Right input`. A disconnected right input (`0`) duplicates the left input into both Capicola channels.
- **Sample** replaces both live inputs with the selected host-catalogued sample. Live audio is not read or mixed while this mode is active.

Changing source resets Capicola's source history. This prevents audio retained from the previous source from sounding after the replacement. During ordinary interaction, entering Sample mode does not implicitly open a file: confirm `Sample` in the Source parameter page or use Capicola's temporary folder/sample selector to open the displayed valid selection. Sample mode stays selected if the card or file is unavailable and supplies silence to Capicola; it never falls back to live input or chooses another sample.

## Loading a sample

1. Put a sample in a location enumerated by disting NT's sample-folder catalogue.
2. Turn the left encoder on the persistent Capicola performance screen to select **Sample** (or set the `Source` parameter).
3. Press the left encoder to temporarily replace the performance screen with **SELECT FOLDER**. Turn to choose a host-catalogued folder, then press to continue.
4. On **SELECT SAMPLE**, turn to choose the displayed host-catalogued sample and press to load it. The persistent performance screen returns immediately, shows **SAMPLE LOAD** during the asynchronous read, then **SAMPLE PLAY** when the loaded buffer starts. **SAMPLE WAIT** means no sample is currently playing.

The folder and sample names shown by the Source page and custom selector come from the host catalogue. `Folder` uses the host string-picker contract and `Sample` uses the host confirm-picker contract, so compatible controllers can present the folder and sample browsers. Changing `Folder` updates the legal `Sample` range and invalidates the prior sample, but does not read a file; confirming `Sample` reads that exact valid catalogue entry into a fixed stereo float buffer and begins one-shot playback from its start. Confirming it again reloads it. An invalid folder or sample value is not clamped to a different catalogue entry, displays no substituted name, and leaves Sample mode selected but silent.

The wrapper requests stereo 32-bit float frames, allowing the host reader to duplicate mono or convert supported PCM formats. It reads at most the first **1,536,000 frames**: 32 seconds at 48 kHz, 64 seconds at 24 kHz, or 16 seconds at 96 kHz. Playback uses linear interpolation and advances by the file-sample-rate/host-sample-rate ratio. It applies the NT sample-player level scaling before both channels enter Capicola.

## Presets, remounts, and unavailable samples

`Source`, `Folder`, and `Sample` are ordinary NT parameters, so the host stores and restores them with the rest of a preset; Capicola adds no separate file database or custom preset format. On a fresh Sample-mode instance, the host's restored parameter callbacks reopen a valid saved selection. If restoration happens before its file is available, the screen remains at **SAMPLE WAIT**; confirm `Sample` after the catalogue is ready.

For real-time safety, the audio callback never scans the catalogue, changes parameter definitions, or reads the SD card. It only reads the fixed memory buffer and runs Capicola. Removing the card after a successful load does not interrupt the current playback. Inserting or remounting the card does not automatically reload a sample from the audio callback; confirm `Sample` or use the temporary folder/sample selector to refresh the catalogue and load the exact selection again.

The API exposes catalogue indices and metadata through `NT_getNumSampleFolders()`, `NT_getSampleFolderInfo()`, and `NT_getSampleFileInfo()`. Capicola invokes those operations and starts `NT_readSampleFrames()` only during construction or a relevant parameter/user-interface event, not while producing an audio block. A persistent request object owns the asynchronous operation. Its callback makes the buffer playable only after a successful read; a newer source or selection generation rejects an obsolete completion.

If the saved folder/sample catalogue entry is missing or moved so the saved indices are no longer valid, the wrapper performs no out-of-range lookup and reads nothing. If metadata is unsupported (including a zero sample rate), it does not start a read. If the host rejects or fails an unreadable resource, the buffer does not become playable. In every case, `Source` remains **Sample**, Capicola's prior source history is cleared, and the outputs are silent; the wrapper neither selects another catalogue entry nor falls back to **Live**. The host owns catalogue construction and error presentation, so the wrapper does not add a recovery dialog or file substitution policy.

Mono files are delivered by the host reader as stereo and therefore feed identical left and right Capicola channels. Stereo files retain left/right order. The wrapper adds no loop, reverse, scrub, region, chopping, polyphony, recording, or live/sample mix feature.

Supported catalogue metadata and format boundaries are recorded in [`CAPABILITY_AUDIT.md`](CAPABILITY_AUDIT.md). Unsupported, unreadable, missing, or unmounted resources retain host behavior.

## Performance controls

The custom performance screen keeps the active **LIVE**/**SAMPLE LOAD**/**SAMPLE PLAY**/**SAMPLE WAIT** source state, three immediate controls, MAIN/ALT state, Mix, and Capicola input/output activity visible. **IN** and **OUT** show the normalized envelope level from 00–99; `!` beside either value reports its transient detector. It follows the approved v2 interaction hierarchy:

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
