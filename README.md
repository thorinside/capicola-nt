# Capicola for disting NT

A planned disting NT wrapper around [Capicola](https://github.com/heavylight-industries/capicola).

## Delivery status

Discovery and required review approval are complete. Implementation is governed by the immutable [approved Spec](docs/APPROVED_SPEC.md) and its [implementation-gate record](docs/IMPLEMENTATION_GATE.md). Work must not expand beyond that first-release boundary without a reviewed Spec revision.

The pinned upstream/platform revisions and traceable first-release processing surface are recorded in the [capability audit](docs/CAPABILITY_AUDIT.md). The buildable plugin implements [normalized live-stereo routing](docs/LIVE_ROUTING.md), mutually exclusive [host-catalogued sample playback](docs/SAMPLE_SOURCE.md), and ordinary [host parameter-to-CV modulation](docs/HOST_MODULATION.md); remaining approved capabilities are delivered incrementally.

## Build and verification

```sh
git submodule update --init --recursive
make verify
```

The ARM plugin object is written to `build/plugins/capicola.o`.
