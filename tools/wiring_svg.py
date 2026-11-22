"""Generates docs/img/wiring.svg (Wokwi-style wiring diagram).

Board drawn from the back side (display facing away), modelled on the Waveshare
interface picture. GPIOs from firmware/include/pins.h, header map from
firmware/CLAUDE.md. Pin labels are the silkscreen labels on the board.
"""
import pathlib

P1 = ["2", "4", "6", "16", "17", "18", "21", "8", "7", "10", "20", "19", "G", "5V"]
P2 = ["3V3", "G", "TX", "RX", "47", "48", "15", "13", "11", "12", "14", "9", "G", "VBAT"]
RED, BLK, YEL, GRN, BLU, PUR, ORG = "#d62828", "#222", "#e0a800", "#2e9e4f", "#2a7de1", "#8e44ad", "#e67e22"
USED1 = {5: YEL, 6: GRN, 13: BLK}
USED2 = {1: RED, 2: BLK, 7: YEL, 8: BLU, 9: PUR, 10: ORG}
X1, X2 = 355, 645


def py(n):
    return 170 + (n - 1) * 28


o = []
a = o.append
a('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 960 740" '
  'font-family="IBM Plex Mono,Menlo,Consolas,monospace" font-size="13">')
a('<style>.bg{fill:#f3f4f2}.tx{fill:#1a1d21}.mu{fill:#5a6068}.wa{fill:#b45309}.ha{stroke:none}'
  '@media (prefers-color-scheme:dark){.bg{fill:#1b1f24}.tx{fill:#e7e9ec}.mu{fill:#9aa1aa}.wa{fill:#f0a43c}.ha{stroke:#8a9096}}</style>')
a('<rect class="bg" width="960" height="740" rx="10"/>')


def wire(d, c):
    if c == BLK:  # Halo, damit schwarze Leitungen auf dunklem Hintergrund sichtbar bleiben
        a(f'<path class="ha" d="{d}" fill="none" stroke-width="7" stroke-linecap="round" stroke-linejoin="round"/>')
    a(f'<path d="{d}" fill="none" stroke="{c}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>')


# --- board, back side ---
a('<rect x="318" y="46" width="364" height="606" rx="26" fill="#2a2c30"/>')  # display frame behind
a('<rect x="330" y="80" width="340" height="520" rx="10" fill="#1d4f9c" stroke="#123a75" stroke-width="2"/>')
for cx, cy in ((352, 102), (648, 102), (352, 578), (648, 578)):
    a(f'<circle cx="{cx}" cy="{cy}" r="11" fill="#c9a227"/><circle cx="{cx}" cy="{cy}" r="5" fill="#2a2c30"/>')
a('<text x="500" y="102" fill="#fff" text-anchor="middle" font-size="12" font-weight="700">ESP32-S3-Touch-LCD-2</text>')
a('<text x="500" y="118" fill="#cfe0ff" text-anchor="middle" font-size="10">back side · display faces away</text>')
a('<rect x="392" y="140" width="72" height="66" rx="3" fill="#2b2d31"/><text x="428" y="177" fill="#bbb" text-anchor="middle" font-size="10">ESP32-S3</text>')
a('<rect x="488" y="216" width="104" height="44" rx="3" fill="#c7cbd0" stroke="#8a9096"/><text x="540" y="242" fill="#555" text-anchor="middle" font-size="10">microSD</text>')
a('<rect x="520" y="300" width="56" height="30" rx="3" fill="#f1f1ee" stroke="#aaa"/><text x="548" y="296" fill="#fff" text-anchor="middle" font-size="10">BAT</text>')
a('<rect x="422" y="420" width="156" height="24" rx="2" fill="#26282c"/>')  # FPC connector
a('<rect x="465" y="520" width="70" height="80" rx="8" fill="#c7cbd0" stroke="#7d838a" stroke-width="2"/>'
  '<rect x="479" y="560" width="42" height="14" rx="7" fill="#7d838a"/>'
  '<text x="500" y="545" fill="#333" text-anchor="middle" font-size="11" font-weight="700">USB-C</text>')
for bx, lbl in ((420, "RST"), (580, "BOOT")):
    a(f'<rect x="{bx-17}" y="572" width="34" height="22" rx="3" fill="#e9e9e6" stroke="#999"/>'
      f'<circle cx="{bx}" cy="583" r="6" fill="#bbb"/><text x="{bx}" y="566" fill="#fff" text-anchor="middle" font-size="10">{lbl}</text>')
a('<rect x="{}" y="156" width="20" height="392" fill="#e3c34a" stroke="#b89a2a"/>'.format(X1 - 10))
a('<rect x="{}" y="156" width="20" height="392" fill="#e3c34a" stroke="#b89a2a"/>'.format(X2 - 10))
a(f'<text x="{X1}" y="148" fill="#fff" text-anchor="middle" font-size="11" font-weight="700">P1</text>')
a(f'<text x="{X2}" y="148" fill="#fff" text-anchor="middle" font-size="11" font-weight="700">P2</text>')
for i, (n1, n2) in enumerate(zip(P1, P2), 1):
    y = py(i)
    for X, name, used, anc, lx in ((X1, n1, USED1, "start", X1 + 16), (X2, n2, USED2, "end", X2 - 16)):
        on = i in used
        a(f'<circle cx="{X}" cy="{y}" r="5" fill="#8a7a3a"/>')
        a(f'<text x="{lx}" y="{y+4}" text-anchor="{anc}" fill="{"#fff" if on else "#9db8e6"}" '
          f'font-weight="{"700" if on else "400"}" font-size="12">{name}</text>')

# --- HX711 ---
a('<rect x="170" y="230" width="120" height="210" rx="6" fill="#2b7a3e" stroke="#1a4f27" stroke-width="2"/>')
a('<text x="230" y="426" fill="#fff" text-anchor="middle" font-weight="700">HX711</text>')
hr = [("VCC", 254), ("SCK", py(5)), ("DT", py(6)), ("GND", 338)]
hl = [("E+", 266, RED), ("E-", 294, BLK), ("A-", 322, "#f0f0f0"), ("A+", 350, GRN), ("B-", 378, None), ("B+", 406, None)]
wire(f"M290,254 H305 V26 H700 V{py(1)}", RED)
wire(f"M290,{py(5)} H{X1}", YEL)
wire(f"M290,{py(6)} H{X1}", GRN)
wire(f"M290,338 H322 V{py(13)} H{X1}", BLK)
for n, y, c in hl:
    if c == "#f0f0f0":
        wire(f"M120,{y} H170", "#555")
        a(f'<path d="M120,{y} H170" stroke="#f0f0f0" stroke-width="2.5"/>')
    elif c:
        wire(f"M120,{y} H170", c)
for n, y in hr:
    a(f'<circle cx="284" cy="{y}" r="4" fill="#f2c94c"/><text x="276" y="{y+4}" fill="#fff" text-anchor="end" font-size="12">{n}</text>')
for n, y, c in hl:
    a(f'<circle cx="176" cy="{y}" r="4" fill="{"#f2c94c" if c else "#9a8a52"}"/>'
      f'<text x="184" y="{y+4}" fill="{"#fff" if c else "#a7c9b0"}" font-size="12">{n}</text>')
a('<text x="230" y="466" text-anchor="middle" font-size="11" class="mu">optional: 100 µF + 100 nF</text>'
  '<text x="230" y="481" text-anchor="middle" font-size="11" class="mu">between VCC and GND</text>')

# --- load cell ---
a('<rect x="30" y="246" width="90" height="124" rx="4" fill="#c9ccd1" stroke="#7d838a" stroke-width="2"/>')
a('<circle cx="58" cy="282" r="9" class="bg" stroke="#7d838a"/><circle cx="92" cy="282" r="9" class="bg" stroke="#7d838a"/>')
a('<text x="75" y="320" text-anchor="middle" font-size="12" fill="#1a1d21">Load cell</text>'
  '<text x="75" y="337" text-anchor="middle" font-size="12" fill="#1a1d21">5 kg</text>')
a('<text x="75" y="392" text-anchor="middle" font-size="11" class="mu">red=E+ black=E-</text>'
  '<text x="75" y="407" text-anchor="middle" font-size="11" class="mu">white=A- green=A+</text>'
  '<text x="75" y="422" text-anchor="middle" font-size="11" class="wa">typical colors,</text>'
  '<text x="75" y="437" text-anchor="middle" font-size="11" class="wa">check yours</text>')

# --- PN532 ---
a('<rect x="760" y="140" width="160" height="310" rx="6" fill="#1b3f8f" stroke="#10275a" stroke-width="2"/>')
a('<rect x="812" y="214" width="92" height="130" rx="10" fill="none" stroke="#d9a441" stroke-width="3"/>')
a('<text x="858" y="276" fill="#fff" text-anchor="middle" font-weight="700">PN532</text>'
  '<text x="858" y="294" fill="#cfd8ef" text-anchor="middle" font-size="11">NFC</text>')
a('<text x="840" y="416" fill="#fff" text-anchor="middle" font-size="11">DIP: SW1=OFF</text>'
  '<text x="840" y="431" fill="#fff" text-anchor="middle" font-size="11">SW2=ON (SPI)</text>')
pr = [("VCC", 1), ("GND", 2), ("SCK", 7), ("MISO", 8), ("MOSI", 9), ("SS", 10)]
for n, p in pr:
    wire(f"M760,{py(p)} H{X2}", USED2[p])
a(f'<circle cx="700" cy="{py(1)}" r="6" fill="{RED}" stroke="#fff" stroke-width="1.5"/>')
for n, p in pr:
    y = py(p)
    a(f'<circle cx="766" cy="{y}" r="4" fill="#f2c94c"/><text x="774" y="{y+4}" fill="#fff" font-size="12">{n}</text>')

# --- notes ---
a('<text x="30" y="686" font-size="12" class="tx">View from the back. Labels as printed on the board. GPIOs: firmware/include/pins.h.</text>')
a('<text x="30" y="704" font-size="12" class="tx">Power everything from 3V3, not 5V. Any G (GND) pin works. 19/20 are USB, do not use.</text>')
a('<text x="30" y="722" font-size="12" class="wa">Not verified: whether GPIO11–15 are used elsewhere on the board.</text>')
a('</svg>')

out = pathlib.Path(__file__).resolve().parent.parent / "docs/img/wiring.svg"
out.write_text("\n".join(o) + "\n")
