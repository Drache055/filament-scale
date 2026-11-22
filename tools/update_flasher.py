#!/usr/bin/env python3
"""Legt eine stabile Firmware für den Web-Flasher unter docs/firmware/ ab.

Aufruf: update_flasher.py <tag> <firmware.factory.bin>

- docs/firmware/<tag>/            Manifest und Image dieser Version
- docs/firmware/versions.json     alle Versionen, neueste zuerst
- docs/firmware/manifest.json     Standard des Flash-Buttons = höchste Version
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


def key(tag):
    return [int(n) for n in re.findall(r"\d+", tag)]


def main():
    tag, src = sys.argv[1], Path(sys.argv[2])
    if not re.fullmatch(r"v\d+(\.\d+)*", tag):
        sys.exit(f"Kein stabiler Versions-Tag: {tag}")

    vdir = ROOT / tag
    vdir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, vdir / BIN)
    write_json(vdir / "manifest.json", manifest(tag))

    vfile = ROOT / "versions.json"
    versions = json.loads(vfile.read_text()) if vfile.exists() else []
    versions = [v for v in versions if v["tag"] != tag]
    versions.append({"tag": tag, "date": date.today().isoformat()})
    versions.sort(key=lambda v: key(v["tag"]), reverse=True)
    write_json(vfile, versions)

    latest = versions[0]["tag"]
    shutil.copyfile(ROOT / latest / BIN, ROOT / BIN)
    write_json(ROOT / "manifest.json", manifest(latest))
    print(f"{tag} abgelegt, Standard: {latest}")


if __name__ == "__main__":
    main()
