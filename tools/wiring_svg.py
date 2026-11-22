# Generates docs/img/wiring.svg. Pins from firmware/include/pins.h, header map from firmware/CLAUDE.md.
P1=["IO2","IO4","IO6","IO16","IO17","IO18","IO21","IO8","IO7","IO10","IO20","IO19","GND","5V"]
P2=["3V3","GND","IO43","IO44","IO47","IO48","IO15","IO13","IO11","IO12","IO14","IO9","GND","VBAT"]
def py(n): return 150+(n-1)*28
X1,X2=380,620
used1={5:"#e0a800",6:"#2e9e4f",13:"#222"}
used2={1:"#d62828",2:"#222",7:"#e0a800",8:"#2a7de1",9:"#8e44ad",10:"#e67e22"}
o=[]
a=o.append
a('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 960 660" font-family="IBM Plex Mono,Menlo,Consolas,monospace" font-size="13">')
a('<rect width="960" height="660" rx="10" fill="#f6f6f2"/>')
# board
a('<rect x="360" y="80" width="280" height="480" rx="12" fill="#1f5e3b" stroke="#123d26" stroke-width="2"/>')
a('<text x="500" y="104" fill="#e9f5ec" text-anchor="middle" font-size="12">Waveshare ESP32-S3-Touch-LCD-2</text>')
a('<rect x="458" y="150" width="84" height="230" rx="4" fill="#10131a" stroke="#0a0c10"/>')
a('<text x="500" y="262" fill="#8a93a3" text-anchor="middle" font-size="12">2" LCD</text><text x="500" y="279" fill="#8a93a3" text-anchor="middle" font-size="11">+ Touch</text>')
a('<rect x="478" y="536" width="44" height="24" rx="3" fill="#b7bcc2" stroke="#6b7177"/><text x="500" y="530" fill="#e9f5ec" text-anchor="middle" font-size="11">USB-C</text>')
a(f'<text x="{X1-7}" y="134" fill="#e9f5ec" text-anchor="start" font-weight="700">P1</text><text x="{X2-14}" y="134" fill="#e9f5ec" text-anchor="end" font-weight="700">P2</text>')
for i,(n1,n2) in enumerate(zip(P1,P2),1):
    y=py(i)
    for X,name,used,anc,lx in ((X1,n1,used1,"start",X1+14),(X2,n2,used2,"end",X2-14)):
        on=i in used
        a(f'<rect x="{X-7}" y="{y-7}" width="14" height="14" fill="#111" /><circle cx="{X}" cy="{y}" r="4" fill="{"#f2c94c" if on else "#9a8a52"}"/>')
        lbl=f"{i} {name}" if anc=="start" else f"{name} {i}"
        a(f'<text x="{lx}" y="{y+4}" text-anchor="{anc}" fill="{"#ffffff" if on else "#8fb39c"}" font-weight="{"700" if on else "400"}" font-size="12">{lbl}</text>')
def wire(d,c): a(f'<path d="{d}" fill="none" stroke="{c}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>')
# HX711
a('<rect x="170" y="210" width="120" height="210" rx="6" fill="#2b7a3e" stroke="#1a4f27" stroke-width="2"/>')
a('<text x="230" y="404" fill="#fff" text-anchor="middle" font-weight="700">HX711</text>')
hr=[("VCC",234,"#d62828"),("SCK",262,"#e0a800"),("DT",290,"#2e9e4f"),("GND",318,"#222")]
hl=[("E+",246,"#d62828"),("E-",274,"#222"),("A-",302,"#f0f0f0"),("A+",330,"#2e9e4f"),("B-",358,None),("B+",386,None)]
# wires first
wire("M290,234 H320 V40 H620 V150","#d62828")
wire("M290,262 H380","#e0a800")
wire("M290,290 H380","#2e9e4f")
wire("M290,318 H340 V486 H380","#222")
for n,y,c in hl:
    if c: wire(f"M120,{y} H170","#555" if c=="#f0f0f0" else c)
    if c=="#f0f0f0": a(f'<path d="M120,{y} H170" stroke="#f0f0f0" stroke-width="2.5"/>')
for n,y,c in hr:
    a(f'<circle cx="284" cy="{y}" r="4" fill="#f2c94c"/><text x="276" y="{y+4}" fill="#fff" text-anchor="end" font-size="12">{n}</text>')
for n,y,c in hl:
    a(f'<circle cx="176" cy="{y}" r="4" fill="{"#f2c94c" if c else "#9a8a52"}"/><text x="184" y="{y+4}" fill="{"#fff" if c else "#a7c9b0"}" font-size="12">{n}</text>')
# load cell
a('<rect x="30" y="226" width="90" height="124" rx="4" fill="#c9ccd1" stroke="#7d838a" stroke-width="2"/>')
a('<circle cx="58" cy="262" r="9" fill="#f6f6f2" stroke="#7d838a"/><circle cx="92" cy="262" r="9" fill="#f6f6f2" stroke="#7d838a"/>')
a('<text x="75" y="300" text-anchor="middle" font-size="12" fill="#1a1d21">Load cell</text><text x="75" y="317" text-anchor="middle" font-size="12" fill="#1a1d21">5 kg</text>')
a('<text x="75" y="372" text-anchor="middle" font-size="11" fill="#5a6068">red=E+ black=E-</text><text x="75" y="387" text-anchor="middle" font-size="11" fill="#5a6068">white=A- green=A+</text><text x="75" y="402" text-anchor="middle" font-size="11" fill="#b45309">typical colors,</text><text x="75" y="417" text-anchor="middle" font-size="11" fill="#b45309">check yours</text>')
a('<text x="230" y="446" text-anchor="middle" font-size="11" fill="#5a6068">optional: 100 µF + 100 nF</text><text x="230" y="461" text-anchor="middle" font-size="11" fill="#5a6068">between VCC and GND</text>')
# PN532
a('<rect x="740" y="120" width="170" height="310" rx="6" fill="#1b3f8f" stroke="#10275a" stroke-width="2"/>')
a('<rect x="790" y="200" width="100" height="150" rx="10" fill="none" stroke="#d9a441" stroke-width="3"/>')
a('<text x="840" y="270" fill="#fff" text-anchor="middle" font-weight="700">PN532</text><text x="840" y="288" fill="#cfd8ef" text-anchor="middle" font-size="11">NFC</text>')
a('<text x="825" y="400" fill="#fff" text-anchor="middle" font-size="11">DIP: SW1=OFF</text><text x="825" y="415" fill="#fff" text-anchor="middle" font-size="11">SW2=ON (SPI)</text>')
pr=[("VCC",1),("GND",2),("SCK",7),("MISO",8),("MOSI",9),("SS",10)]
for n,p in pr:
    y=py(p); wire(f"M740,{y} H{X2}",used2[p])
a(f'<circle cx="{X2}" cy="150" r="6" fill="#d62828" stroke="#fff" stroke-width="1.5"/>')
for n,p in pr:
    y=py(p); a(f'<circle cx="746" cy="{y}" r="4" fill="#f2c94c"/><text x="754" y="{y+4}" fill="#fff" font-size="12">{n}</text>')
# notes
a('<text x="30" y="604" font-size="12" fill="#1a1d21">Pins: firmware/include/pins.h · header numbering per Waveshare schematic (check the silkscreen).</text>')
a('<text x="30" y="622" font-size="12" fill="#1a1d21">Power everything from 3V3 (P2 pin 1), not 5V. Any GND pin works. IO19/IO20 are USB.</text>')
a('<text x="30" y="640" font-size="12" fill="#b45309">Not verified: whether IO11–IO15 are used elsewhere on the board (probably the unused SD slot).</text>')
a('</svg>')
import pathlib; open(pathlib.Path(__file__).resolve().parent.parent/"docs/img/wiring.svg","w").write("\n".join(o)+"\n")
