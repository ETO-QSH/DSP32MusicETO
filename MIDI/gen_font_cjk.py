# -*- coding: utf-8 -*-
import io, os, re, glob
from PIL import Image, ImageDraw, ImageFont
P = r"C:\Users\ETO\workspace_ccstheia\DSP32Project"
LRC = glob.glob(r"D:\Downloads\Downloads\*.lrc")[0]
txt = open(LRC, "rb").read().decode("utf-8-sig")
ts = re.compile(r"\[(\d+):(\d+)(?:[.:](\d+))?\]")
OPEN, CLOSE = "\u3016", "\u3017"
lines = []
for l in txt.splitlines():
    l = l.strip()
    if not l or ts.match(l) is None: continue
    body = ts.sub("", l).strip()
    i, j = body.find(OPEN), body.rfind(CLOSE)
    if i >= 0 and j > i:
        s = body[i+1:j].strip()
        if s: lines.append(s)
chars, seen = [], set()
for s in lines:
    for ch in s:
        if ch not in seen and ord(ch) > 0x20:
            seen.add(ch); chars.append(ch)
CW = CH = 14
base = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 13)
def glyph(ch):
    img = Image.new("L", (CW, CH), 0)
    ImageDraw.Draw(img).text((CW/2.0, CH/2.0), ch, fill=255, font=base, anchor="mm")
    px = img.load(); rows = []
    for r in range(CH):
        v = 0
        for c in range(CW):
            if px[c, r] > 100: v |= (1 << c)
        rows.append(v)
    return rows
rows = [(ord(c), c, glyph(c)) for c in chars]
rows.sort(key=lambda x: x[0])            # ★ 关键：按码点升序，二分查找才成立
OUT = P + r"\HardWare\OLED\font16.h"
with io.open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("/* font16.h -- 14x14 CJK subset from the LRC lyrics (Chinese lines)\n")
    f.write("   %d glyphs, SORTED BY CODE (binary search); 14 U16 per glyph, row-major, bit0 = left */\n" % len(rows))
    f.write("#ifndef FONT16_H_\n#define FONT16_H_\n\n")
    f.write("#define FONT_W 14U\n#define FONT_H 14U\n")
    f.write("#define FONT16_COUNT %dU\n\n" % len(rows))
    f.write("typedef struct { U16 code; U16 rows[14]; } Glyph14;\n\n")
    f.write("static const Glyph14 font16[FONT16_COUNT] = {\n")
    for code, ch, g in rows:
        f.write("    { 0x%04X, { %s } },\n" % (code, ",".join("0x%04X" % v for v in g)))
    f.write("};\n\n#endif\n")
ok = all(rows[i][0] < rows[i+1][0] for i in range(len(rows)-1))
print("font16.h regenerated: %d glyphs, sorted=%s, %d bytes" % (len(rows), ok, os.path.getsize(OUT)))