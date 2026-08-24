# License audit

- Audit date: 2026-08-24
- Audited wrapper tree: the release tree containing this document
- Audited upstream revision: Capicola `f0fb61cfa7111067b4ec1a642d1b16a0910adb3b`
- Audited platform API revision: distingNT API `cd12d876dbe060859828053efab1cbc98c9df251`

## Conclusion

The applicable upstream license is the **GNU Affero General Public License, version 3** (`AGPL-3.0-only`). The wrapper is released under that same license. The pinned Capicola headers are compiled directly into the plugin object, so the public object-code release must be accompanied by equivalent access to complete machine-readable Corresponding Source under AGPLv3 section 6. No concrete license incompatibility was found.

The MIT-licensed distingNT API headers may be combined with and distributed as part of the AGPL-covered wrapper provided their copyright and permission notice is retained. The wrapper does not compile or link Capicola's nested Alchemy SDK or libDaisy dependencies.

This is a project compliance record, not a general legal opinion.

## Inputs audited

| Component | Pinned source | License evidence | Used by plugin |
|---|---|---|---|
| Wrapper source and build scripts | this repository | root `LICENSE`, `NOTICE` | Yes |
| Capicola | `vendor/capicola` at `f0fb61c…` | upstream `LICENSE`; upstream README license section | Yes, header-only DSP core |
| distingNT API | `vendor/distingNT_API` at `cd12d87…` | upstream `LICENSE` | Yes, API headers |
| Alchemy SDK / libDaisy | nested below Capicola | upstream gitlink and README | No |

The audit searched the pinned Capicola tree for license, copyright, author, and additional-term notices. Capicola's root license is the unmodified AGPLv3 text. Its README identifies the project and Heavylight Industries authorship and declares AGPLv3. No separate Capicola copyright file, exception, additional restriction, or alternative license was found in the DSP headers used by this wrapper.

## Distribution obligations and implementation

| Obligation found | Release implementation |
|---|---|
| License the combined wrapper under the same AGPLv3 terms | Root `LICENSE` is byte-for-byte identical to pinned `vendor/capicola/LICENSE`; wrapper source uses `SPDX-License-Identifier: AGPL-3.0-only`. |
| Preserve license text, existing notices, absence-of-warranty notice, and upstream attribution | Root `LICENSE`, `NOTICE`, `THIRD_PARTY_NOTICES.md`, and the complete pinned Capicola submodule tree are included in Corresponding Source. |
| Prominently identify modification/integration and its date | `NOTICE` identifies this as an independently maintained disting NT wrapper first published in 2026; README identifies the wrapper relationship. The upstream submodule itself is not modified. |
| Make machine-readable Corresponding Source available with network-distributed object code at no extra charge | Every release must attach the source archive produced by `make source-package` next to the object-code download and link it as directed by `docs/SOURCE_OFFER.md`. |
| Include source needed to generate and install the object | The source package includes wrapper source, tests, build scripts, documentation, both complete pinned top-level submodule trees, license files, and generated provenance. Installation information is documented in the release/player documentation; no secret key is required. |
| Retain the distingNT API MIT copyright and permission notice | `LICENSES/distingNT_API-MIT.txt` is copied byte-for-byte from the pinned API submodule and included in the source package; `THIRD_PARTY_NOTICES.md` attributes Expert Sleepers Ltd. |
| Do not impose further restrictions | Release materials make no additional license, EULA, or field-of-use restriction. |
| Provide Appropriate Legal Notices if a modified upstream interactive UI already did so | The audited upstream interface does not display such notices. The wrapper adds no remote-network interface; legal notices remain prominently available in release materials and source. |

## Corresponding-source boundary

The generated archive includes the complete tracked trees of both top-level vendor submodules, not merely gitlinks. This avoids the common omission in automatically generated GitHub source snapshots. Capicola's nested `lib/alchemy-sdk` gitlink is preserved as provenance but its contents are not required to build this plugin and are not part of this plugin's Corresponding Source.

A recipient can build the plugin from the archive with the documented toolchain by running `make plugin`; no network checkout is needed for the two dependencies used by the build. Maintainers run the Git-metadata provenance checks in `make verify` before creating the archive.

## Release review

Before any public release:

1. Run `make release-assets` from the exact release commit.
2. Confirm the source archive's generated `SOURCE_PROVENANCE.txt` names that commit and both pinned vendor revisions.
3. Push an owner-approved `v*` tag for that exact commit. `.github/workflows/release.yml` verifies the tag and publishes `capicola.o` and `capicola-nt-source.tar.gz` as adjacent assets of the same GitHub release.
4. Confirm the release notes link `LICENSE`, `NOTICE`, `THIRD_PARTY_NOTICES.md`, `docs/SOURCE_OFFER.md`, and `SUBMODULES.lock` at that release commit.
5. Download both published assets and recheck the source provenance and plugin object rather than relying only on the workflow result.
6. If either vendor revision changes, repeat this audit and update `SUBMODULES.lock` and the verification expectations before release.

These are mandatory license checks, not speculative rights-holder clearance or an invented audio-quality gate.
