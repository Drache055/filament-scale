#!/usr/bin/env python3
"""Vergleicht frisch gerenderte Screens pixelgenau mit den Referenzen. Aufruf: check.py <gerendert/1x> <referenz/1x>"""
import os
import sys

from PIL import Image, ImageChops

new_dir, ref_dir = sys.argv[1:3]
new = {f for f in os.listdir(new_dir) if f.endswith(".png")}
ref = {f for f in os.listdir(ref_dir) if f.endswith(".png")}
bad = []
for f in sorted(new | ref):
    if f not in ref:
        bad.append(f"{f}: fehlt in den Referenzen")
    elif f not in new:
        bad.append(f"{f}: wird nicht mehr gerendert")
    else:
        a = Image.open(os.path.join(new_dir, f)).convert("RGB")
        b = Image.open(os.path.join(ref_dir, f)).convert("RGB")
        if a.size != b.size or ImageChops.difference(a, b).getbbox():
            bad.append(f"{f}: Bild weicht ab")
for line in bad:
    print(line)
print(f"{len(new)} gerendert, {len(bad)} Abweichungen")
sys.exit(1 if bad else 0)
