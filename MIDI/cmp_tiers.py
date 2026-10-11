import re, bisect
src = open(r"C:\Users\ETO\workspace_ccstheia\DSP32Project\User\song_tables.h", encoding='utf-8').read()
def arr(n):
    m = re.search(n + r'\[\]\s*=\s*\{(.*?)\};', src, re.S)
    return [int(x) for x in re.findall(r'(\d+)U', m.group(1))]
def hz(m): return 440.0 * 2.0 ** ((m - 69) / 12.0)
def events(words, base, shift):
    i = 0
    while i + 1 < len(words):
        w0, w1 = words[i], words[i+1]; i += 2
        if w0 == 0 and w1 == 0: break
        if w0 == 0: continue
        vs = [w0 & 0x3F, (w0 >> 6) & 0x3F, (w0 >> 12) & 0x3F, w1 & 0x3F]
        fs = [hz(base + v - 1 + shift) for v in vs if v]
        u = (w1 >> 6) & 0xFF
        if fs: yield max(fs), u * 250.0 / 3.0
SETS = [("A 均衡   440/659/831 ", (440, 659, 831)),
        ("B 候选   330/660/1000", (330, 660, 1000)),
        ("C 现状   330/660/1320", (330, 660, 1320))]
DATA = {}
for name, base, shift in (("左手", 27, 24), ("右手", 52, 0)):
    DATA[name] = list(events(arr("s_song_left" if name == "左手" else "s_song_right"), base, shift))
DATA["合并"] = DATA["左手"] + DATA["右手"]

def dist(ev, b):
    tot = sum(d for _, d in ev); out = []
    edges = [0] + list(b) + [10**9]
    for i in range(4):
        dd = sum(d for m, d in ev if edges[i] <= m < edges[i+1])
        out.append(100.0 * dd / tot)
    return out

for lab, b in SETS:
    print("== %s ==" % lab)
    for hand in ("左手", "右手", "合并"):
        ev = DATA[hand]
        d = dist(ev, b)
        print("   %s  时长占比: x1 %4.1f%%  x2 %4.1f%%  x4 %4.1f%%  x8 %4.1f%%" %
              (hand, d[0], d[1], d[2], d[3]))
    print()