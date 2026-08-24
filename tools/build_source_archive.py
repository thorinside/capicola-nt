#!/usr/bin/env python3
"""Build a deterministic corresponding-source archive with expanded submodules."""

from __future__ import annotations

import gzip
import io
import os
import pathlib
import subprocess
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "build/release/capicola-nt-source.tar.gz"
PREFIX = "capicola-nt-source"
SUBMODULES = ("vendor/capicola", "vendor/distingNT_API")


def run(cwd: pathlib.Path, *args: str) -> str:
    return subprocess.run(
        list(args), cwd=cwd, check=True, text=True, capture_output=True
    ).stdout.strip()


def tracked_files(repo: pathlib.Path) -> list[tuple[str, int]]:
    entries: list[tuple[str, int]] = []
    output = run(repo, "git", "ls-files", "--stage")
    for line in output.splitlines():
        metadata, path = line.split("\t", 1)
        mode, _object_id, stage = metadata.split()
        if stage != "0" or mode == "160000":
            continue
        entries.append((path, int(mode[-3:], 8)))
    return entries


def add_bytes(archive: tarfile.TarFile, name: str, data: bytes, mode: int = 0o644) -> None:
    info = tarfile.TarInfo(f"{PREFIX}/{name}")
    info.size = len(data)
    info.mode = mode
    info.mtime = 0
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    archive.addfile(info, io.BytesIO(data))


def add_tree(
    archive: tarfile.TarFile, repo: pathlib.Path, archive_prefix: str = ""
) -> None:
    for relative, mode in tracked_files(repo):
        source = repo / relative
        target = f"{archive_prefix}{relative}"
        if source.is_symlink():
            info = tarfile.TarInfo(f"{PREFIX}/{target}")
            info.type = tarfile.SYMTYPE
            info.linkname = os.readlink(source)
            info.mode = mode
            info.mtime = 0
            archive.addfile(info)
        else:
            add_bytes(archive, target, source.read_bytes(), mode)


def provenance() -> bytes:
    lines = [
        "Capicola for disting NT corresponding source",
        f"wrapperCommit={run(ROOT, 'git', 'rev-parse', 'HEAD')}",
    ]
    for submodule in SUBMODULES:
        lines.append(
            f"{submodule}={run(ROOT / submodule, 'git', 'rev-parse', 'HEAD')}"
        )
    lines.extend(
        (
            "license=AGPL-3.0-only",
            "build=make plugin",
            "",
        )
    )
    return "\n".join(lines).encode("utf-8")


def main() -> None:
    for submodule in SUBMODULES:
        if not (ROOT / submodule / ".git").exists():
            raise SystemExit(f"initialize required submodule first: {submodule}")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w") as archive:
                add_tree(archive, ROOT)
                for submodule in SUBMODULES:
                    add_tree(archive, ROOT / submodule, f"{submodule}/")
                add_bytes(archive, "SOURCE_PROVENANCE.txt", provenance())

    with tarfile.open(OUTPUT, "r:gz") as archive:
        names = set(archive.getnames())
        required = {
            f"{PREFIX}/LICENSE",
            f"{PREFIX}/NOTICE",
            f"{PREFIX}/Makefile",
            f"{PREFIX}/src/capicola_nt.cpp",
            f"{PREFIX}/vendor/capicola/LICENSE",
            f"{PREFIX}/vendor/capicola/lib/KeyframeRecorder.h",
            f"{PREFIX}/vendor/distingNT_API/LICENSE",
            f"{PREFIX}/vendor/distingNT_API/include/distingnt/api.h",
            f"{PREFIX}/SOURCE_PROVENANCE.txt",
        }
        missing = required - names
        if missing:
            raise SystemExit(f"source archive is incomplete: {sorted(missing)}")

    digest = run(ROOT, "shasum", "-a", "256", str(OUTPUT)).split()[0]
    print(f"built {OUTPUT.relative_to(ROOT)} sha256={digest}")


if __name__ == "__main__":
    main()
