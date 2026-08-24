# Third-party notices

## Capicola

This project is a wrapper around [Capicola](https://github.com/heavylight-industries/capicola), authored and published by **Heavylight Industries**.

- Pinned revision: `f0fb61cfa7111067b4ec1a642d1b16a0910adb3b`
- Repository path: `vendor/capicola`
- License: GNU Affero General Public License, version 3 (`AGPL-3.0-only`)
- License text: [`LICENSE`](LICENSE), preserved byte-for-byte from the pinned upstream [`vendor/capicola/LICENSE`](vendor/capicola/LICENSE)
- Upstream notices and project history: retained in the pinned submodule, including `vendor/capicola/README.md`

The wrapper is an independently maintained disting NT integration. It does not claim to be an official Heavylight Industries release and does not deliberately redesign Capicola's processing behavior.

## distingNT API

The build uses API headers from [Expert Sleepers' distingNT API](https://github.com/expertsleepersltd/distingNT_API).

- Pinned revision: `cd12d876dbe060859828053efab1cbc98c9df251`
- Repository path: `vendor/distingNT_API`
- Copyright: Copyright (c) 2025 Expert Sleepers Ltd
- License: MIT
- Preserved license text: [`LICENSES/distingNT_API-MIT.txt`](LICENSES/distingNT_API-MIT.txt)

No source from Capicola's nested Alchemy SDK dependency is compiled or linked into the disting NT plugin. Its gitlink remains in the complete pinned upstream tree for provenance.
