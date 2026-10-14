# -*- coding: utf-8 -*-
import io, os, re, glob
from PIL import Image, ImageDraw, ImageFont

W = r"D:\Documents\Documents\deepseek-harness\default-workspace"
P = r"C:\Users\ETO\workspace_ccstheia\DSP32Project"
LRC = glob.glob(r"D:\Downloads\Downloads\*.lrc")[0]
txt = open(LRC, "rb").read().decode("utf-8-sig")

ts = re.compile(r"\[(\d+):(\d+)(?:[.:](\d+))?\]")
def ms_of(l):
    m = ts.match(l)
    if not m: return None
    c = (m.group(3) or "0"); c = (c + "00")[:3]
    return int(m.group(1)) * 60000 + int(m.group(2)) * 1000 + int(c)

OPEN, CLOSE = "\u3016", "\u3017"          # 〖 〗
rows = []
for l in txt.splitlines():
    l = l.strip()
    if not l: continue
    t = ms_of(l)
    if t is None: continue
    body = ts.sub("", l).strip()
    if not body: continue
    i, j = body.find(OPEN), body.rfind(CLOSE)
    if (i >= 0) and (j > i):
        text = body[i + 1:j].strip()
        if text: rows.append((t, text))
    # lines without 〖〗 are the japanese/furigana half -> skipped on purpose

rep = io.open(os.path.join(W, "lrc_report.txt"), "w", encoding="utf-8", newline="\n")
rep.write("Chinese lyric lines kept: %d\n\n" % len(rows))
rep.write("%-9s %-4s %s\n" % ("time(ms)", "len", "text"))
rep.write("-" * 78 + "\n")
for t, s in rows:
    rep.write("%-9d %-4d %s\n" % (t, len(s), s))
rep.close()
print("kept Chinese lines:", len(rows))
print("max line length:", max(len(s) for _, s in rows))
print("lines needing 2 rows (>8):", sum(1 for _, s in rows if len(s) > 8))

chars, seen = [], set()
for _, s in rows:
    for ch in s:
        if ch not in seen and ord(ch) > 0x7E:
            seen.add(ch); chars.append(ch)
print("unique glyphs:", len(chars))

fpath = r"C:\Windows\Fonts\msyh.ttc"
base = ImageFont.truetype(fpath, 15)
def glyph(ch):
    img = Image.new("L", (16, 16), 0)
    ImageDraw.Draw(img).text((8, 8), ch, fill=255, font=base, anchor="mm")
    px = img.load(); g = []
    for half in (0, 8):
        for c in range(16):
            b = 0
            for r in range(8):
                if px[c, r + half] > 100: b |= (1 << r)
            g.append(b)
    return g
rows2 = [(ord(c), c, glyph(c)) for c in chars]
OUT = P + r"\HardWare\OLED\font16.h"
with io.open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("/* font16.h -- 16x16 Chinese subset, extracted from the LRC lyrics\n")
    f.write("   %d glyphs; 32 bytes each: bytes 0..15 = top page columns, 16..31 = bottom page\n" % len(rows2))
    f.write("   column-major, LSB = top row of that page (SSD1306 native) */\n")
    f.write("#ifndef FONT16_H_\n#define FONT16_H_\n\n#define FONT16_COUNT %dU\n\n" % len(rows2))
    f.write("typedef struct { U16 code; U8 data[32]; } Glyph16;\n\n")
    f.write("static const Glyph16 font16[FONT16_COUNT] = {\n")
    for code, ch, g in rows2:
        f.write("    { 0x%04X, { %s } },\n" % (code, ",".join("0x%02X" % b for b in g)))
    f.write("};\n\n#endif\n")
print("font16.h:", len(rows2), "glyphs,", os.path.getsize(OUT), "bytes")

TAB = P + r"\HardWare\OLED\lrc_table.h"
with io.open(TAB, "w", encoding="utf-8", newline="\n") as f:
    f.write("/* lrc_table.h -- lyric timings (ms) + UTF-8 text; generated, do not edit */\n")
    f.write("#ifndef LRC_TABLE_H_\n#define LRC_TABLE_H_\n\n#define LRC_COUNT %dU\n\n" % len(rows))
    f.write("typedef struct { U32 t_ms; const char *txt; } LrcLine;\n\n")
    f.write("static const LrcLine lrc_table[LRC_COUNT] = {\n")
    for t, s in rows:
        f.write('    { %7dU, "%s" },\n' % (t, s.replace('\\', '\\\\').replace('"', '\\"')))
    f.write("};\n\n#endif\n")
print("lrc_table.h:", len(rows), "lines,", os.path.getsize(TAB), "bytes")