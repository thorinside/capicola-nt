# Corresponding source and installation information

Capicola for disting NT is free software released under the GNU Affero General Public License, version 3. There is no warranty. You may copy, modify, and convey it under the terms in [`LICENSE`](../LICENSE).

## Getting the exact source

Every public object-code release provides `capicola-nt-source.tar.gz` beside the `capicola.o` plugin download, at no additional charge. The tag-only release workflow attaches both files to the same GitHub release and links these license, notice, attribution, source-access, and provenance materials in its release notes. The source archive is the preferred form for modification and contains:

- all wrapper source, tests, documentation, and build scripts;
- the complete pinned Capicola source used by the plugin;
- the complete pinned distingNT API headers used by the plugin;
- the AGPLv3 and MIT license texts, notices, and attribution; and
- `SOURCE_PROVENANCE.txt`, recording the release commit and submodule revisions.

GitHub's automatically generated “Source code” archives do **not** expand Git submodules and are not the release's corresponding-source download. Use the explicitly attached `capicola-nt-source.tar.gz` archive.

The same source can be obtained from a Git checkout with:

```sh
git clone --recurse-submodules <release-repository-url>
cd <release-checkout>
git checkout <release-tag>
git submodule update --init --recursive
```

The release page supplies the repository URL and exact tag. Exact dependency URLs and commits are also recorded in [`SUBMODULES.lock`](../SUBMODULES.lock).

## Build and install a modified version

The source archive requires no submodule download. With a C++ compiler, Python 3, and the Arm GNU C++ toolchain available:

```sh
make plugin
```

This creates `build/plugins/capicola.o` without a network checkout. The maintainer-only `make verify` target additionally checks Git metadata and therefore runs in the tagged Git checkout before the source archive is produced.

Copy the object to `/programs/plug-ins/capicola.o` on the disting NT MicroSD card using the normal user-accessible plugin installation workflow, then scan/load the plugin on a compatible module. No project-specific authorization key or password is required to build or install a modified object. Firmware compatibility remains limited to the baseline documented for the release.

For source-access problems, use the release repository's public issue tracker and include the release tag, archive filename, and observed error.

## Release maintainer check

An owner-approved `v*` tag triggers `.github/workflows/release.yml`. Before tagging, run `make release-assets`; after the workflow succeeds, download both assets from the public release and verify them. Tagging and publication require explicit owner approval.
