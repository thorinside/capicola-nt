# Capicola for disting NT

An independently maintained disting NT wrapper around [Capicola](https://github.com/heavylight-industries/capicola), authored by Heavylight Industries. This is not an official upstream Capicola release.

## Delivery status

Discovery and required review approval are complete. Implementation is governed by the immutable [approved Spec](docs/APPROVED_SPEC.md) and its [implementation-gate record](docs/IMPLEMENTATION_GATE.md). Work must not expand beyond that first-release boundary without a reviewed Spec revision.

The pinned upstream/platform revisions and traceable first-release processing surface are recorded in the [capability audit](docs/CAPABILITY_AUDIT.md). The buildable plugin implements [normalized live-stereo routing](docs/LIVE_ROUTING.md), mutually exclusive [host-catalogued sample playback](docs/SAMPLE_SOURCE.md), ordinary [host parameter-to-CV modulation](docs/HOST_MODULATION.md), and default-disconnected [analysis CV routing](docs/ANALYSIS_CV.md); remaining approved capabilities are delivered incrementally.

## Build and verification

```sh
git submodule update --init --recursive
make verify
```

The ARM plugin object is written to `build/plugins/capicola.o`.

## License and source

Capicola for disting NT and the incorporated Capicola DSP are free software released under the **GNU Affero General Public License, version 3** (`AGPL-3.0-only`). There is no warranty. See [LICENSE](LICENSE) for the complete terms, [NOTICE](NOTICE) and [third-party notices](THIRD_PARTY_NOTICES.md) for attribution, and the completed [license audit](docs/LICENSE_AUDIT.md) for the audited obligations.

Each object-code release must offer the explicitly attached `capicola-nt-source.tar.gz` corresponding-source archive at no extra charge. GitHub's automatic source snapshots omit submodule contents and are not a substitute. See the [corresponding-source instructions](docs/SOURCE_OFFER.md). Release maintainers build and check the archive with:

```sh
make source-package
```
