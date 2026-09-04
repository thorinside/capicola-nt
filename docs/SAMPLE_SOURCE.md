# Live and streamed-sample sources

This delivery uses the disting NT firmware/SDK **1.16.0** sample catalogue and streaming API. It does not scan arbitrary files itself.

## Selecting a source

The `Source` parameter has two mutually exclusive values:

- **Live** processes `Left input` and `Right input`. A disconnected right input (`0`) duplicates the left input into both Capicola channels.
- **Sample** replaces both live inputs with the selected host-catalogued sample. Live audio is not read or mixed while this mode is active.

Changing source resets Capicola's source history. A fixed 10 ms linear bridge blends the last audible Capicola output voltage into the new output, preserving the endpoint across Live's 5x and Sample's 8x output gains. Only that held endpoint survives the reset; the old source is no longer processed. Entering Sample mode immediately opens the displayed valid folder/sample selection as a stream and retains its existing 50 ms initial fade-in. Sample mode stays selected if the card or file is unavailable and settles to silence; it never falls back to live input.

## Selecting a sample

1. Put a sample in a location enumerated by disting NT's sample-folder catalogue.
2. Turn the left encoder on the persistent Capicola performance screen to select **Sample** (or set the `Source` parameter). The current valid selection starts streaming immediately.
3. Press the left encoder to temporarily replace the performance screen with **SELECT FOLDER**. Turn to choose a host-catalogued folder, then press to continue.
4. On **SELECT SAMPLE**, turn to choose the displayed host-catalogued sample and press **LOAD**. The persistent performance screen returns immediately and shows **CAPICOLA** followed by the currently audible sample's compact name. During a replacement it keeps the old name until the zero-gain handoff, then changes to the new name. Repeated trailing audio file extensions are removed; stems longer than 32 characters retain 16 leading and 13 trailing characters around a middle ellipsis. **CAPICOLA WAIT** means no sample stream is currently available.

The folder and sample names shown by the Source page and custom selector come from the host catalogue. `Folder` uses the host string-picker contract and `Sample` uses the host confirm-picker contract, so compatible controllers can present the folder and sample browsers. Changing `Folder` updates the legal `Sample` range and, in Sample mode, immediately opens the selected valid entry in that folder. If the old Sample number is outside the new folder's range, Capicola updates the visible host parameter to the nearest legal value before opening; recursive host callbacks are guarded. Folder changes and catalogue refreshes, including a change in SD mount state, preserve the engine and its audio history while Live is selected. Pressing **LOAD** in Capicola's sample selector deliberately reopens that exact valid catalogue entry and restarts playback.

The NT stream renderer supplies stereo normalized float frames, duplicates mono as needed, and advances by the file-sample-rate/host-sample-rate ratio. Capicola streams the full host-supported file rather than truncating it to a construction-time buffer. Those normalized frames enter the Capicola engine directly; the established NT sample-player 8x level scaling is applied only after processing. This preserves the existing dry sample output level without overdriving the engine input. There is no transport trigger input; samples loop from frame zero while Sample mode remains active. The renderer is authoritative for the opened stream: valid returned frames continue to play even after the selected catalogue entry's reported frame count, since the firmware may resolve a physical variant with a different length.

Once a stream has returned its first frames, a short render at or beyond the reported boundary permits a loop reopen. If it stops returning frames before that boundary, the wrapper reopens it after 100 ms of missing host frames. Any returned frames reset the missing-frame count, as do a reopen or replacement handoff. Initial loading waits for first progress without repeatedly reopening. Recovery permits at most one reopen per audio block, so files shorter than one block can leave the remainder silent instead of causing unbounded SD work.

API v13 returns a rendered-frame count without a separate EOF indicator. The wrapper therefore cannot distinguish a shorter physical variant from a stalled stream: a shorter variant can have a 100 ms gap before looping, and an underrun lasting at least 100 ms may restart playback. A prepared loop boundary remains useful, but does not guarantee seamless playback. The wrapper adds no loop-point editor or boundary crossfade.

Opening a valid replacement sample preserves the already-warm Capicola engine. The old stream continues until a separately prepared stream returns and buffers its first frames. Output then follows the old stream down to silence over 50 ms, changes to the replacement's frame zero at that exact midpoint, and follows the new stream up to unity over the following 50 ms. The title changes at the same midpoint, so it continues to identify the audible stream. The phases meet inside one audio block, so only the midpoint sample is forced to zero; there is no early input discontinuity, empty first stream block, or longer wet-path warm-up gap. Reopening the same sample with **LOAD** uses the same transition. Selecting the still-audible sample again cancels a pending replacement. If a replacement is missing or refused, the last valid output instead fades to silence without a fade-in. This bounded click-safety fade is fixed and separate from Capicola's **Fade** processing control. Entering Sample mode from Live remains a true source replacement and resets the engine.

## Presets, remounts, and unavailable samples

`Source`, `Folder`, and `Sample` are ordinary NT parameters, so the host stores and restores them with the rest of a preset; Capicola adds no separate file database or custom preset format. On a fresh Sample-mode instance, the host's restored parameter callbacks reopen a valid saved selection exactly once, even when the host notifies all three parameters. If restoration happens before its file is available, the screen remains at **CAPICOLA WAIT**; reopen the selector and press **LOAD** after the catalogue is ready.

For real-time safety, the audio callback never scans the catalogue, changes parameter definitions, allocates memory, or performs unbounded recovery. Its only SD operation is `NT_streamRender()` and at most one `NT_streamOpen()` per block for looping or the stalled-stream recovery described above. Reaching the reported count alone never triggers a reopen while the renderer continues supplying frames. A short read before that boundary leaves the affected remainder silent and the next block may retry; reaching 100 ms without progress permits a restart. Removing the card interrupts streaming. After inserting or remounting it, press **LOAD** in the sample selector if playback does not resume.

The API exposes catalogue indices and metadata through `NT_getNumSampleFolders()`, `NT_getSampleFolderInfo()`, and `NT_getSampleFileInfo()`. Capicola invokes those operations and initially calls `NT_streamOpen()` only during a relevant parameter/user-interface event. The two per-instance stream states and host buffers use the exact byte sizes published in `NT_globals`; the wrapper reserves no full-file storage.

If the saved Folder catalogue entry is missing or moved so its index is no longer valid, the wrapper performs no out-of-range lookup and opens nothing. A valid folder synchronizes Sample into that folder's visible legal range. If metadata is unsupported (including a zero sample rate), it does not open a stream. If the host rejects an unreadable resource, the stream does not become playable. In these failure cases, `Source` remains **Sample** and the output settles to silence through the applicable transition; the wrapper never falls back to **Live**. The host owns catalogue construction and error presentation, so the wrapper does not add a recovery dialog or hidden file substitution policy.

Mono files are delivered by the host stream as stereo and therefore feed identical left and right Capicola channels. Stereo files retain left/right order. The wrapper adds no reverse, scrub, region, chopping, polyphony, recording, or live/sample mix feature.

Supported catalogue metadata and format boundaries are recorded in [`CAPABILITY_AUDIT.md`](CAPABILITY_AUDIT.md). Unsupported, unreadable, missing, or unmounted resources retain host behavior.

## Performance controls

The custom performance screen shows **CAPICOLA LIVE**, **CAPICOLA** plus the compact playing sample stem, or **CAPICOLA WAIT**. It keeps three immediate controls and Mix visible without the redundant MAIN/ALT or input/output activity numbers. It follows the approved interaction hierarchy:

- Main pots: **Stretch**, **Threshold**, **Feedback**.
- Press any of the three pots to switch the complete bank to the clearly labelled alternate trio: **Pitch**, **Grain Size**, **Quality**. Each pot then waits until it crosses its current displayed target or comes within 0.005 of it on the normalized sweep. The bank-press callback itself does not change pot values. The three active control names and values identify the bank without a separate MAIN/ALT label; initial screen entry retains the host's normal takeover behavior.
- Turn the right encoder for **Mix**; press it for the audited momentary **Slice** action. Slice is ignored until the engine has processed its first block, so pressing it while an initial sample stream is waiting cannot cancel engine startup.
- Turn the left encoder to replace Live with Sample or Sample with Live. In Sample mode, press it to enter the temporary folder/sample selection described above.
- The temporary selector uses only the left encoder. Pressing after a sample choice opens it and returns to the one persistent performance screen; pots and the right encoder do not change performance controls while selection is open.

No separate module buttons are claimed by the custom UI. All twelve upstream continuous controls plus the wrapper's **Input Gain** are ordinary NT parameters on the **Performance** page, so host parameter-to-CV mapping remains available. Input Gain is an attenuation-only -60–0 dB control, defaults to 0 dB, affects both the dry and wet paths in Live and Sample modes, and smooths changes over about 10 ms. The persistent screen keeps the approved v2 immediate/alternate hierarchy above; the host parameter view exposes the five secondary controls without adding a routine performance page or materially changing that screen:

- **Envelope Smoothing:** 0.00–100.00% normalized sweep maps exponentially to cutoff 0.00005–0.125 (about 1.2 Hz–3 kHz at 48 kHz); default 42.59% (about 0.0014).
- **Fade:** 0.00–100.00% maps exponentially to 10–250 ms (480–12000 frames at 48 kHz); default 21.53% (about 20 ms).
- **Drive:** 0.00–100.00% maps linearly to 0.5–4.0; default 14.29% (about 1.0).
- **Drive Character:** 0.00% quake, 50.00% clean, 100.00% sinc; default 100.00%.
- **Feedback Tone:** 0.00–100.00% maps exponentially to normalized bandpass center 0.002–0.9 (about 48 Hz–21.6 kHz at 48 kHz); default 37.69% (about 0.02).

These controls use the exact audited upstream sweep equations, are shared by the left and right Capicola channels (Feedback Tone controls both feedback filters), and affect Live and Sample through the same processing path. The existing controls retain their audited ranges and tapers: Pitch ±12 semitones, Stretch realtime-to-freeze `(1-x)^2.5`, Threshold ratio 0–8/top=mute, Grain Size 32–4096 keyframes, Quality ε 0.1–0.001, Feedback 0–1.5, and Mix dry-to-wet. The Stretch value is displayed as its time factor (about 5.7× at midpoint) and **FREEZE** at the clockwise stop. For an uninterrupted long stretch, use 100% Mix and 100% Threshold: lower Threshold values intentionally accept transients that catch the read head up to the current source. A larger Grain Size increases the distance between adaptive splices but does not extend the maximum beyond freeze. Together with the right-encoder **Slice** action, the interface represents all 13 audited upstream processing capabilities; Input Gain is NT wrapper gain staging, not a new DSP capability.

## Processing and routing

Both source modes enter the same stereo-linked Capicola processing path and use the same confirmed controls, audio outputs, and Add/Replace output modes. At 0 dB Input Gain and 0% Mix, either source retains its existing dry output level through the normalized DSP boundary once a source transition has completed. The upstream stereo guard is retained: if exactly one channel automatically catches a transient and the source-grid lags differ by more than 48,000 frames (one second at 48 kHz), the other channel receives a Slice for its next block. Below that threshold the channels remain independent. Source replacement resets the engine, reapplies the current processing values, and bridges the output over 10 ms as described above. These wrapper fixes do not expand the audited processing-capability set or make sample playback an upstream Capicola sampler feature.
