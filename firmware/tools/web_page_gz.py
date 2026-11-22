# PlatformIO-Pre-Skript: packt die Weboberfläche aus src/net/web_page.h mit gzip und erzeugt
# $BUILD_DIR/gen/web_page_gz.h (WEB_PAGE_GZ, WEB_PAGE_GZ_LEN, WEB_PAGE_ETAG).
# Der Server schickt die gepackte Seite (~26 statt ~90 KB) und antwortet bei gleichem ETag mit 304.
# Auch direkt aufrufbar: python3 tools/web_page_gz.py <web_page.h> <Ausgabe.h>
import gzip
import hashlib
import os
import sys

START = 'R"HTML('
END = ')HTML"'


def page_bytes(src_path):
    with open(src_path, encoding="utf-8") as f:
        text = f.read()
    a = text.index(START) + len(START)
    b = text.rindex(END)
    return text[a:b].encode("utf-8")


def header_text(html):
    gz = gzip.compress(html, compresslevel=9, mtime=0)  # mtime=0: gleiche Seite, gleiche Bytes
    etag = hashlib.sha256(html).hexdigest()[:16]
    lines = [
        "#pragma once",
        "// Automatisch erzeugt von tools/web_page_gz.py, nicht von Hand ändern.",
        "#include <stddef.h>",
        "#include <stdint.h>",
        '#define WEB_PAGE_ETAG "\\"%s\\""' % etag,
        '#define WEB_PAGE_ETAG_GZ "\\"%s-gz\\""' % etag,
        "static const size_t WEB_PAGE_GZ_LEN = %d;" % len(gz),
        "static const uint8_t WEB_PAGE_GZ[] = {",
    ]
    for i in range(0, len(gz), 20):
        lines.append("  " + ",".join("0x%02x" % c for c in gz[i : i + 20]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n", len(html), len(gz)


def generate(src_path, out_path):
    text, plain, packed = header_text(page_bytes(src_path))
    old = None
    if os.path.exists(out_path):
        with open(out_path, encoding="utf-8") as f:
            old = f.read()
    if old != text:  # nur bei Änderung schreiben, sonst baut SCons webserver.cpp jedes Mal neu
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(text)
    print("Webseite: %d Bytes, gzip %d Bytes" % (plain, packed))


try:
    Import("env")  # noqa: F821 (von PlatformIO bereitgestellt)
except NameError:  # direkter Aufruf ohne PlatformIO
    generate(sys.argv[1], sys.argv[2])
else:
    gen_dir = os.path.join(env.subst("$BUILD_DIR"), "gen")  # noqa: F821
    generate(os.path.join(env.subst("$PROJECT_SRC_DIR"), "net", "web_page.h"),  # noqa: F821
             os.path.join(gen_dir, "web_page_gz.h"))
    env.Append(CPPPATH=[gen_dir])  # noqa: F821
