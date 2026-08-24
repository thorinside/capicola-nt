#!/usr/bin/env python3
"""Verify the pinned license, notice, and vendor-provenance release materials."""

from __future__ import annotations

import hashlib
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXPECTED = {
    "vendor/capicola": (
        "https://github.com/heavylight-industries/capicola.git",
        "f0fb61cfa7111067b4ec1a642d1b16a0910adb3b",
    ),
    "vendor/distingNT_API": (
        "https://github.com/expertsleepersltd/distingNT_API.git",
        "cd12d876dbe060859828053efab1cbc98c9df251",
    ),
}


def fail(message: str) -> None:
    print(f"license verification failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def git(*args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, check=True, text=True, capture_output=True
    )
    return result.stdout.strip()


def require_text(path: str, fragments: tuple[str, ...]) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    for fragment in fragments:
        if fragment not in text:
            fail(f"{path} is missing required text: {fragment!r}")


def main() -> None:
    wrapper_license = (ROOT / "LICENSE").read_bytes()
    upstream_license = (ROOT / "vendor/capicola/LICENSE").read_bytes()
    if wrapper_license != upstream_license:
        fail("root LICENSE is not byte-for-byte identical to Capicola LICENSE")

    api_notice = (ROOT / "LICENSES/distingNT_API-MIT.txt").read_bytes()
    upstream_api_notice = (ROOT / "vendor/distingNT_API/LICENSE").read_bytes()
    if api_notice != upstream_api_notice:
        fail("preserved distingNT API MIT license differs from pinned upstream")

    lock_lines = {
        line
        for line in (ROOT / "SUBMODULES.lock").read_text(encoding="utf-8").splitlines()
        if line and not line.startswith("#")
    }
    expected_lock_lines = {
        f"{path} {url} {commit}" for path, (url, commit) in EXPECTED.items()
    }
    if lock_lines != expected_lock_lines:
        fail("SUBMODULES.lock does not exactly match audited dependencies")

    stage = {}
    for line in git("ls-files", "--stage", "vendor").splitlines():
        metadata, path = line.split("\t", 1)
        mode, object_id, _stage = metadata.split()
        stage[path] = (mode, object_id)

    for path, (url, commit) in EXPECTED.items():
        if stage.get(path) != ("160000", commit):
            fail(f"{path} is not a gitlink pinned to {commit}")
        configured_url = git("config", "-f", ".gitmodules", f"submodule.{path}.url")
        if configured_url != url:
            fail(f"{path} URL is {configured_url!r}, expected {url!r}")
        checked_out = git("-C", path, "rev-parse", "HEAD")
        if checked_out != commit:
            fail(f"{path} checkout is {checked_out}, expected {commit}")

    require_text(
        "NOTICE",
        (
            "independently maintained disting NT wrapper around\nCapicola",
            "GNU\nAffero General Public License, version 3",
            EXPECTED["vendor/capicola"][1],
            "copyright (c) 2025\nExpert Sleepers Ltd",
        ),
    )
    require_text(
        "README.md",
        (
            "independently maintained disting NT wrapper",
            "GNU Affero General Public License, version 3",
            "docs/LICENSE_AUDIT.md",
            "docs/SOURCE_OFFER.md",
        ),
    )
    require_text(
        "docs/LICENSE_AUDIT.md",
        (
            "AGPL-3.0-only",
            "No concrete license incompatibility was found.",
            "make source-package",
        ),
    )
    require_text(
        "docs/SOURCE_OFFER.md",
        (
            "capicola-nt-source.tar.gz",
            "do **not** expand Git submodules",
            "owner-approved `v*` tag",
        ),
    )
    require_text(
        ".github/workflows/release.yml",
        (
            'tags:\n      - "v*"',
            "make clean verify",
            "make source-package",
            "build/plugins/capicola.o",
            "build/release/capicola-nt-source.tar.gz",
            "gh release create",
            "--verify-tag",
            "LICENSE",
            "NOTICE",
            "THIRD_PARTY_NOTICES.md",
            "docs/SOURCE_OFFER.md",
            "SUBMODULES.lock",
        ),
    )
    require_text(
        "Makefile",
        ("release-assets: verify source-package",),
    )

    for source in ("src/capicola_nt.cpp", "include/capicola_nt/live_path.h"):
        first_lines = "\n".join(
            (ROOT / source).read_text(encoding="utf-8").splitlines()[:8]
        )
        if "SPDX-License-Identifier: AGPL-3.0-only" not in first_lines:
            fail(f"{source} lacks a prominent AGPL SPDX notice")

    print(
        "license release verified: "
        f"AGPL sha256={hashlib.sha256(wrapper_license).hexdigest()}, "
        "notices and pinned vendor provenance are consistent"
    )


if __name__ == "__main__":
    main()
