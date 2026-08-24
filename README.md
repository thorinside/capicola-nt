# Capicola for disting NT

Capicola is a stereo time-stretching and pitch-shifting effect for the
[Expert Sleepers disting NT](https://www.expert-sleepers.co.uk/distingNT.html).
It can process live audio or play a sample from the NT's SD card through the
same Capicola engine.

This is an independently maintained disting NT wrapper around
[Capicola by Heavylight Industries](https://github.com/heavylight-industries/capicola),
not an official Heavylight Industries release.

- **Current release:** [v0.5.0](https://github.com/thorinside/capicola-nt/releases/tag/v0.5.0)
- **Supported baseline:** disting NT firmware 1.16.0, plug-in API v13
- **Plug-in GUID:** `ThCa`

Developers and reviewers can find the build, architecture, provenance, and
release information in the shorter [technical reference](docs/TECHNICAL.md).

## Install

### With nt_helper

1. Download [`capicola.o`](https://github.com/thorinside/capicola-nt/releases/latest/download/capicola.o).
2. Connect the disting NT to your computer and open `nt_helper`.
3. Open **Plugin Manager** and choose **Install from File**.
4. Select the downloaded `capicola.o`. `nt_helper` uploads it to
   `/programs/plug-ins/` and asks the NT to rescan its plug-ins.
5. On the disting NT, add an algorithm and select **Capicola** from the effect
   plug-ins.

If Capicola does not appear in the NT algorithm list after installation, rescan
or restart the NT and confirm that the module is running the supported firmware
baseline.

### Manual installation

1. Download `capicola.o` from the
   [latest release](https://github.com/thorinside/capicola-nt/releases/latest).
2. Copy it to `/programs/plug-ins/` on the disting NT SD card.
3. Rescan plug-ins or restart the disting NT.
4. Add a **Capicola** algorithm to a preset.

The adjacent `capicola-nt-source.tar.gz` release asset is the corresponding
source archive; it is not the installable plug-in.

## Start with live audio

The factory routing is ready for a basic stereo patch:

| Parameter | Default | Meaning |
| --- | --- | --- |
| Left input | Input 1 | Left live source |
| Right input | Input 2 | Right live source |
| Left output | Bus 13 / output 1 | Processed left output |
| Right output | Bus 14 / output 2 | Processed right output |
| Output modes | Add | Add Capicola to anything already on those buses |
| Source | Live | Process the selected input buses |

1. Patch a stereo source to inputs 1 and 2. For mono, set **Right input** to
   `None`; the left input will feed both processing channels.
2. Listen to outputs 1 and 2.
3. Begin with **Stretch** at 0%, **Feedback** at 0%, and **Mix** at 100%.
4. If these output buses are dedicated to Capicola, change both output modes to
   **Replace**. Leave them on **Add** only when you intentionally want to mix
   Capicola with other algorithms on the same buses.

Live and Sample are replacement source modes. They are never mixed together
inside Capicola.

## Use the performance screen

Capicola keeps its normal controls on one screen. The three pots have a MAIN
bank and an ALT bank:

| Hardware control | MAIN | ALT |
| --- | --- | --- |
| Left pot | Stretch | Pitch |
| Centre pot | Threshold | Grain Size |
| Right pot | Feedback | Quality |
| Press any pot | Switch the whole pot bank between MAIN and ALT | Switch back |
| Left encoder | Select Live or Sample | — |
| Press left encoder | In Sample mode, open the folder/sample selector | — |
| Right encoder | Mix | — |
| Press right encoder | Trigger Slice | — |

The bottom line says **MAIN** or **ALT** and shows the current Mix. **IN** and
**OUT** are envelope levels from 00–99; `!` after either value means its
transient detector fired.

The source label has three states:

- **LIVE** — the selected input buses are being processed.
- **SAMPLE PLAY** — a sample stream is open.
- **SAMPLE WAIT** — Sample mode is selected, but no sample stream is open.

## Load and play a sample

Capicola uses the sample catalogue provided by the disting NT. It does not scan
arbitrary paths itself.

1. Put the sample in a folder that appears in the NT sample catalogue.
2. Turn the left encoder until the source changes to **Sample**.
3. Press the left encoder. On **SELECT FOLDER**, turn to choose a folder and
   press to continue.
4. On **SELECT SAMPLE**, turn to choose a sample and press to load it.
5. The performance screen returns. **SAMPLE PLAY** confirms that the stream
   opened; **SAMPLE WAIT** means it did not.

Samples play once, forward, from the beginning. To restart the selected sample,
confirm the **Sample** parameter again or repeat the selection. Mono samples
feed both channels; stereo samples keep their left/right order.

The Source, Folder, and Sample selections are ordinary NT preset parameters.
Keep the SD sample catalogue stable when a preset depends on a sample. If the
saved catalogue indices are invalid or the selected file is unreadable,
Capicola remains in Sample mode and outputs silence—it does not switch to Live.

Capicola does not add looping, reverse playback, scrubbing, regions, chopping,
polyphony, recording, or live/sample mixing.

## Controls

The hardware screen presents the six primary controls above. The remaining
continuous controls are on the NT **Performance** parameter page.

| Control | Range/action | Default | What it does |
| --- | ---: | ---: | --- |
| Pitch | -12 to +12 semitones | 0 | Transposes the processed audio |
| Stretch | 0–100% | 0% | Moves from real-time playback to freeze |
| Threshold | 0–100% | 22% | Sets the automatic transient threshold; 100% disables automatic triggers |
| Grain Size | 32–4096 | 128 | Sets the analysis grain length in keyframes |
| Quality | 0–100% | 100% | Moves from coarse to fine analysis |
| Feedback | 0–150% | 0% | Feeds processed audio back into the engine |
| Envelope Smoothing | 0–100% | 42.59% | Moves from slower/smoother to faster envelope tracking |
| Fade | 0–100% | 21.53% | Maps from a short 10 ms fade to a long 250 ms fade |
| Drive | 0–100% | 14.29% | Maps to 0.5–4.0 keyframe drive |
| Drive Character | 0–100% | 100% | 0% Quake, 50% Clean, 100% Sinc |
| Mix | 0–100% | 100% | Dry to wet |
| Feedback Tone | 0–100% | 37.69% | Moves the feedback band-pass centre from low to high |
| Slice | Right encoder press | — | Forces a manual splice |

Feedback above 100% can grow rapidly and become loud. Reduce Feedback and your
monitoring level before experimenting in that range.

## CV modulation and analysis outputs

There are no dedicated Pitch CV, Stretch CV, or other processing-control input
parameters. To modulate a continuous control, select it in the disting NT
parameter-mapping interface and assign a CV source. Mapped values work in both
Live and Sample modes.

The **Routing** page also provides four optional analysis outputs. All are
`None` by default:

| Output selector | Signal |
| --- | --- |
| Input Transient output | 0 V or 5 V transient gate |
| Output Transient output | 0 V or 5 V transient gate |
| Input Envelope output | 0–5 V envelope |
| Output Envelope output | 0–5 V envelope |

Assign each signal to an unused bus, or select `None` to disconnect it. Analysis
outputs replace their selected bus, so do not assign two analysis signals—or an
analysis signal and an audio output—to the same bus.

## Troubleshooting

- **Capicola is not in the algorithm list:** confirm `capicola.o` is in
  `/programs/plug-ins/`, then rescan plug-ins or restart the NT.
- **No sound in Live mode:** check Left/Right input and output routing, set Mix
  to 100%, Stretch to 0%, Feedback to 0%, and use Replace on dedicated outputs.
- **A mono patch is only on the left:** set Right input to `None` to normalize
  Left input to both channels.
- **The display says SAMPLE WAIT:** make sure the SD card is mounted and the
  selected folder/sample still exists, then select the sample again.
- **A sample stops:** sample playback is intentionally one-shot; confirm the
  Sample parameter again to restart it.
- **The level keeps increasing:** set Feedback below 100%, preferably to 0%
  while diagnosing the patch.
- **CV is not changing a control:** use the NT parameter-mapping interface;
  Capicola does not expose dedicated modulation input parameters.

When reporting a problem, include the Capicola release, disting NT firmware
version, Source mode, routing, sample details if applicable, and a minimal
preset or reproduction sequence.

## License and attribution

Capicola for disting NT and the incorporated Capicola DSP are free software
under the [GNU Affero General Public License, version 3](LICENSE). There is no
warranty. See [NOTICE](NOTICE), [third-party notices](THIRD_PARTY_NOTICES.md),
the completed [license audit](docs/LICENSE_AUDIT.md), the
[corresponding-source instructions](docs/SOURCE_OFFER.md), and the
[technical reference](docs/TECHNICAL.md) for attribution, corresponding source,
and pinned dependency details.
