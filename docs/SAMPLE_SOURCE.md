# Live and loaded-sample sources

This delivery uses the disting NT firmware/SDK **1.16.0** sample catalogue and stream APIs. It does not scan arbitrary files itself.

## Selecting a source

The `Source` parameter has two mutually exclusive values:

- **Live** processes `Left input` and `Right input`. A disconnected right input (`0`) duplicates the left input into both Capicola channels.
- **Sample** replaces both live inputs with the selected host-catalogued sample. Live audio is not read or mixed while this mode is active.

Changing source resets Capicola's source history. This prevents audio retained from the previous source from sounding after the replacement. Sample mode stays selected if the card or file is unavailable and supplies silence to Capicola; it never falls back to live input or chooses another sample.

## Loading a sample

1. Put a sample in a location enumerated by disting NT's sample-folder catalogue.
2. Set `Source` to **Sample**.
3. Choose `Folder`, then confirm `Sample`.

The folder and sample names shown by the parameters come from the host catalogue. A selection opens as a one-shot stream from its beginning. Selecting it again reopens it. The wrapper requests normal sequential, forward playback and uses the file sample rate to ask the host streamer for rate conversion to the 48 kHz baseline.

Mono files are delivered by the host stream as stereo and therefore feed identical left and right Capicola channels. Stereo files retain left/right order. The wrapper adds no duration limit, loop, reverse, scrub, region, chopping, polyphony, recording, or live/sample mix feature.

Supported catalogue metadata and format boundaries are recorded in [`CAPABILITY_AUDIT.md`](CAPABILITY_AUDIT.md). Unsupported, unreadable, missing, or unmounted resources retain host behavior.

## Processing and routing

Both source modes enter the same two-channel Capicola processing path and use the same audio outputs and Add/Replace output modes. This slice does not change the audited processing-capability set or claim that loaded-sample playback is an upstream Capicola sampler feature.
