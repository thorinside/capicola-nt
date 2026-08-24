#!/usr/bin/env python3
"""Verify that the audited first-release capability ledger remains reproducible."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_SPEC_SHA256 = "39dd5357c55bd5baee57b13d5c2105bba85cc1bcd7130eb79b750a33338e6253"
EXPECTED_EXCLUSIONS = {
    "recording",
    "reverse",
    "scrubbing",
    "start/end editing",
    "loop-point editing",
    "loop-boundary crossfade",
    "slicing/chopping",
    "polyphonic playback",
    "live/sample mixing",
    "silent sample substitution",
}


def fail(message: str) -> None:
    raise AssertionError(message)


def git(*args: str) -> str:
    return subprocess.check_output(
        ["git", *args], cwd=ROOT, text=True, stderr=subprocess.STDOUT
    ).strip()


def verify_gitlink(path: str, expected: str) -> None:
    fields = git("ls-files", "-s", "--", path).split()
    if len(fields) < 4 or fields[0] != "160000":
        fail(f"{path} is not a staged Git submodule gitlink")
    if fields[1] != expected:
        fail(f"{path} gitlink is {fields[1]}, expected {expected}")
    actual = git("-C", path, "rev-parse", "HEAD")
    if actual != expected:
        fail(f"{path} checkout is {actual}, expected {expected}")


def verify_locator(locator: str) -> None:
    try:
        relative, token = locator.split(":", 1)
    except ValueError as exc:
        raise AssertionError(f"invalid source locator: {locator}") from exc
    path = ROOT / relative
    if not path.is_file():
        fail(f"source locator file is missing: {relative}")
    if token not in path.read_text(encoding="utf-8"):
        fail(f"source locator token {token!r} is missing from {relative}")


def main() -> int:
    spec_digest = hashlib.sha256((ROOT / "docs/APPROVED_SPEC.md").read_bytes()).hexdigest()
    if spec_digest != EXPECTED_SPEC_SHA256:
        fail(f"approved Spec digest is {spec_digest}, expected {EXPECTED_SPEC_SHA256}")

    ledger = json.loads((ROOT / "docs/capability-set.json").read_text(encoding="utf-8"))
    if ledger.get("schemaVersion") != 1:
        fail("unsupported capability ledger schema")

    upstream = ledger["upstream"]
    baseline = ledger["platformBaseline"]
    verify_gitlink("vendor/capicola", upstream["commit"])
    verify_gitlink("vendor/distingNT_API", baseline["sdkCommit"])

    if baseline["firmware"] != "1.16.0" or baseline["sdkTag"] != "v1.16.0":
        fail("firmware and SDK baseline must remain exactly 1.16.0/v1.16.0")
    if baseline["apiVersion"] != 13 or baseline["sampleRateHz"] != 48000:
        fail("audited platform constants changed")

    api_header = (ROOT / "vendor/distingNT_API/include/distingnt/api.h").read_text(
        encoding="utf-8"
    )
    match = re.search(r"kNT_apiVersionCurrent\s*=\s*kNT_apiVersion(\d+)", api_header)
    if not match or int(match.group(1)) != baseline["apiVersion"]:
        fail("SDK kNT_apiVersionCurrent does not match the ledger")

    ids: set[str] = set()
    capabilities = ledger["processingCapabilities"]
    if len(capabilities) != 13:
        fail("the audited processing surface must contain exactly 13 capabilities")
    for capability in capabilities:
        capability_id = capability["id"]
        if capability_id in ids:
            fail(f"duplicate capability id: {capability_id}")
        ids.add(capability_id)
        if not capability.get("label") or not capability.get("range"):
            fail(f"{capability_id} lacks a label or range")
        if not capability.get("ntLimit"):
            fail(f"{capability_id} lacks an NT-specific limit")
        evidence = capability.get("upstreamEvidence", [])
        if not evidence:
            fail(f"{capability_id} lacks upstream evidence")
        for locator in evidence:
            verify_locator(locator)

    signals = ledger["analysisSignals"]
    if {signal["id"] for signal in signals} != {
        "inputTransient",
        "outputTransient",
        "inputEnvelope",
        "outputEnvelope",
    }:
        fail("analysis signal set changed")
    for signal in signals:
        verify_locator(signal["upstreamEvidence"])
        if "selector 0 means disconnected" not in signal["ntLimit"]:
            fail(f"{signal['id']} does not retain the disconnected-output limit")

    sample_boundary = ledger["sampleSourceBoundary"]
    if set(sample_boundary["notSamplerFeatures"]) != EXPECTED_EXCLUSIONS:
        fail("sample-source exclusions changed")

    excluded_ids = {item["id"] for item in ledger["excludedUpstreamSurface"]}
    if not {"customModulationMatrix", "dedicatedTriggerCv", "recordAndPlaybackTransport"} <= excluded_ids:
        fail("required excluded upstream surfaces are missing")

    print(
        f"capability audit verified: {len(capabilities)} processing capabilities, "
        f"{len(signals)} analysis signals, API v{baseline['apiVersion']}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, subprocess.CalledProcessError, KeyError, ValueError) as exc:
        print(f"capability audit verification failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
