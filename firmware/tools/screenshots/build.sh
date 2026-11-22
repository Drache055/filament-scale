#!/bin/sh
# Baut den Screenshot-Treiber (echte UI-Quellen + Stubs) und erzeugt PNGs in ../../screenshots (1x und 4x).
set -e
cd "$(dirname "$0")"
ROOT=../..
LVGL=$ROOT/.pio/libdeps/waage/lvgl
JSON=$ROOT/.pio/libdeps/waage/ArduinoJson/src
BUILD=${TMPDIR:-/tmp}/shots_build
OUT=${OUT_DIR:-$ROOT/screenshots}  # OUT_DIR=... SHOT_LANG=en sh build.sh für englische Screens
mkdir -p "$BUILD" "$OUT/1x" "$OUT/4x"

LVOBJ=$BUILD/lvgl.a
if [ ! -f "$LVOBJ" ]; then
  echo "LVGL übersetzen (einmalig) ..."
  mkdir -p "$BUILD/lv"
  find "$LVGL/src" "$ROOT/src/ui/fonts" -name '*.c' > "$BUILD/lv.list"
  # md5 gibt es nur unter macOS, unter Linux heißt es md5sum.
  if command -v md5 >/dev/null 2>&1; then HASH=md5; else HASH=md5sum; fi
  cat > "$BUILD/cc.sh" <<CC
#!/bin/sh
o=\$(echo "\$1" | $HASH | cut -d' ' -f1)
clang -c -O1 -w -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -I$PWD/$ROOT/include -I$PWD/$LVGL -I$PWD/stubs -I$PWD/$ROOT/src/ui -o $BUILD/lv/\$o.o "\$1"
CC
  chmod +x "$BUILD/cc.sh"
  xargs -n1 -P 8 "$BUILD/cc.sh" < "$BUILD/lv.list"
  ar rcs "$LVOBJ" "$BUILD"/lv/*.o
fi

clang++ -std=gnu++17 -O1 -w -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -DDATA_DIR=\"$PWD/$ROOT/data\" \
  -Istubs -I. -I$ROOT/include -I$ROOT/src -I$ROOT/src/ui -I$LVGL -I$JSON \
  main.cpp stubs.cpp $ROOT/src/i18n/i18n.cpp $ROOT/src/ui/theme.cpp $ROOT/src/nfc/tags.cpp $ROOT/src/nfc/ndef.cpp $ROOT/src/core/weigh_core.cpp \
  "$LVOBJ" -o "$BUILD/shots"

rm -rf "$BUILD/ppm" && mkdir -p "$BUILD/ppm"
SHOT_LANG=${SHOT_LANG:-de} "$BUILD/shots" "$BUILD/ppm" > /dev/null
rm -f "$OUT"/1x/*.png "$OUT"/4x/*.png
python3 - "$BUILD/ppm" "$OUT" <<'PY'
import sys, glob, os
from PIL import Image
src, out = sys.argv[1:3]
for p in sorted(glob.glob(src + "/*.ppm")):
    name = os.path.basename(p)[:-4]
    im = Image.open(p)
    im.save(f"{out}/1x/{name}.png")
    im.resize((960, 1280), Image.NEAREST).save(f"{out}/4x/{name}.png")
    print(name)
PY
