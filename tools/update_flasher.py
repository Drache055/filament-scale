#!/usr/bin/env python3
"""Legt eine Firmware für den Web-Flasher unter docs/firmware/ ab.

Aufruf: update_flasher.py <tag> <firmware.factory.bin> [--prerelease]

- docs/firmware/<tag>/            Manifest und Image dieser Version
- docs/firmware/versions.json     alle Versionen mit Kanal (stable, rc, test), neueste zuerst
- docs/firmware/manifest.json     Standard des Flash-Buttons = höchste stabile Version

Kanäle wie beim Online-Update (firmware/CLAUDE.md 12.6): stable = Tag ohne Zusatz und keine
Pre-Release auf GitHub, rc = Zusatz -rc oder Pre-Release ohne Zusatz, test = jeder andere Zusatz.
Pre-Releases ändern den Standard nie.
"""
import json
import re
import shutil
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "docs" / "firmware"
BIN = "firmware.factory.bin"


def manifest(version):
    return {
        "name": "Filament Scale",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32-S3", "parts": [{"path": BIN, "offset": 0}]}],
    }


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")


def channel(tag, github_prerelease):
    suffix = tag.partition("-")[2]
    if suffix:
        return "rc" if re.match(r"rc(\d|$)", suffix) else "test"
    return "rc" if github_prerelease else "stable"


RANK = {"test": 0, "rc": 1, "stable": 2}


def key(entry):
    """Sortierschlüssel: Versionsnummer, bei gleicher Nummer test < rc < stable, dann Zahl am Ende."""
    base, _, suffix = entry["tag"].partition("-")
    nums = [int(n) for n in re.findall(r"\d+", base)]
    tail = [int(n) for n in re.findall(r"\d+", suffix)]
    return nums, RANK[entry["channel"]], tail


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    github_pre = "--prerelease" in sys.argv[1:]
    if len(args) != 2:
        sys.exit("Aufruf: update_flasher.py <tag> <firmware.factory.bin> [--prerelease]")
    tag, src = args[0], Path(args[1])
    if not re.fullmatch(r"v\d+(\.\d+)*(-[0-9A-Za-z.]+)?", tag):
        sys.exit(f"Kein Versions-Tag: {tag}")
    ch = channel(tag, github_pre)

    vdir = ROOT / tag
    vdir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, vdir / BIN)
    write_json(vdir / "manifest.json", manifest(tag))

    vfile = ROOT / "versions.json"
    versions = json.loads(vfile.read_text()) if vfile.exists() else []
    for v in versions:
        v.setdefault("channel", channel(v["tag"], False))
    versions = [v for v in versions if v["tag"] != tag]
    versions.append({"tag": tag, "date": date.today().isoformat(), "channel": ch})
    versions.sort(key=key, reverse=True)
    write_json(vfile, versions)

    stable = [v for v in versions if v["channel"] == "stable"]
    if stable:
        latest = stable[0]["tag"]
        shutil.copyfile(ROOT / latest / BIN, ROOT / BIN)
        write_json(ROOT / "manifest.json", manifest(latest))
        print(f"{tag} ({ch}) abgelegt, Standard: {latest}")
    else:
        print(f"{tag} ({ch}) abgelegt, Standard unverändert")


if __name__ == "__main__":
    main()
