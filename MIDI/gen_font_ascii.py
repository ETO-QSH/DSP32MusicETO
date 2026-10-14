# -*- coding: utf-8 -*-
from PIL import Image, ImageDraw, ImageFont
import os, shutil
OUT = r"C:\Users\ETO\workspace_ccstheia\DSP32Project\MIDI\font8x8.h"
path = r"C:\Windows\Fonts\consola.ttf"
CELL = 8
# 8px: cap height ~6px -> fits an 8x8 cell with margin
base = ImageFont.truetype(path, 8)

def glyph(ch):
    img = Image.new("L", (CELL, CELL), 0)
    d = ImageDraw.Draw(img)
    d.text((CELL // 2, CELL // 2), ch, fill=255, font=base, anchor="mm")
    px = img.load()
    cols = []
    for c in range(CELL):
        b = 0
        for r in range(CELL):
            if px[c, r] > 100:
                b |= (1 << r)
        cols.append(b)
    return cols

rows = [(ch, glyph(ch)) for ch in (chr(c) for c in range(0x20, 0x7F))]
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("/* font8x8.h -- 8x8 ASCII font (0x20..0x7E), column-major, LSB = top row\n")
    f.write("   rasterised from a system monospace font (Pillow, 8px, anchor=mm) */\n")
    f.write("#ifndef FONT8X8_H_\n#define FONT8X8_H_\n\n")
    f.write("static const U8 font8x8[95][8] = {\n")
    for ch, cols in rows:
        esc = '\\"' if ch == '"' else ('\\\\' if ch == '\\' else ch)
        f.write("    { %s },   /* '%s' */\n" % (", ".join("0x%02X" % c for c in cols), esc))
    f.write("};\n\n#endif\n")
print("font regenerated at 8px:", OUT)
for ch in "A8gKW":
    cols = dict(rows)[ch]
    print("  '%s':" % ch)
    for r in range(8):
        print("    " + "".join("#" if (cols[c] >> r) & 1 else "." for c in range(8)))