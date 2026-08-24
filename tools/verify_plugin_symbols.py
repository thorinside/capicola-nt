#!/usr/bin/env python3
"""Validate NT imports and loader-facing callback section layout."""

from __future__ import annotations

import pathlib
import subprocess
import sys


ALLOWED_UNDEFINED = {
    "NT_algorithmIndex",
    "NT_drawText",
    "NT_getNumSampleFolders",
    "NT_getSampleFileInfo",
    "NT_getSampleFolderInfo",
    "NT_globals",
    "NT_isSdCardMounted",
    "NT_parameterOffset",
    "NT_setParameterFromUi",
    "NT_streamOpen",
    "NT_streamRender",
    "NT_updateParameterDefinition",
    "_GLOBAL_OFFSET_TABLE_",
    "exp2f",
    "memcpy",
    "memset",
    "powf",
    "sinf",
    "snprintf",
    "sqrtf",
    "strlen",
    "strncpy",
    "tanf",
}

LOADER_CALLBACKS = {
    "calculateRequirements(",
    "construct(",
    "customUi(",
    "draw(",
    "hasCustomUi(",
    "parameterChanged(",
    "parameterString(",
    "pluginEntry",
    "setupUi(",
    "step(",
}


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} PLUGIN")

    plugin = pathlib.Path(sys.argv[1])
    output = subprocess.run(
        ["arm-none-eabi-nm", "-u", str(plugin)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    symbols = {line.split()[-1] for line in output.splitlines() if line.split()}
    unexpected = sorted(symbols - ALLOWED_UNDEFINED)
    if unexpected:
        print("unexpected undefined plug-in symbols:", file=sys.stderr)
        for symbol in unexpected:
            print(f"  {symbol}", file=sys.stderr)
        return 1

    symbol_table = subprocess.run(
        ["arm-none-eabi-objdump", "-t", "-C", str(plugin)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    callback_sections: dict[str, str] = {}
    for line in symbol_table.splitlines():
        fields = line.split(None, 5)
        if len(fields) != 6 or fields[2] != "F":
            continue
        section, name = fields[3], fields[5]
        for callback in LOADER_CALLBACKS:
            if callback == "pluginEntry":
                matches = name == callback
            else:
                matches = callback in name
            if matches:
                callback_sections[callback] = section

    missing_callbacks = sorted(LOADER_CALLBACKS - callback_sections.keys())
    split_callbacks = sorted(
        callback
        for callback, section in callback_sections.items()
        if section != ".text"
    )
    if missing_callbacks or split_callbacks:
        if missing_callbacks:
            print(
                f"missing loader callbacks: {', '.join(missing_callbacks)}",
                file=sys.stderr,
            )
        if split_callbacks:
            print("loader callbacks must share .text:", file=sys.stderr)
            for callback in split_callbacks:
                print(
                    f"  {callback} -> {callback_sections[callback]}",
                    file=sys.stderr,
                )
        return 1

    print(
        f"plug-in imports: {len(symbols)} known NT/runtime symbols; "
        f"{len(callback_sections)} loader callbacks in .text"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
