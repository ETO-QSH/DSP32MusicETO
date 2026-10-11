import re, bisect
src = open(r"C:\Users\ETO\workspace_ccstheia\DSP32Project\User\song_tables.h", encoding='utf-8').read()
def arr(name):
    m = re.search(name + r'\[\]\s*=\s*\{(.*?)\};', src, re.S)
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
        if fs: yield max(fs), u * 250.0 / 3.0, min(fs)
TIERS = [(0,330,"x1 (--330)"),(330,660,"x2 (330-660)"),(660,1320,"x4 (660-1320)"),(1320,99999,"x8 (1320--)")]
pool = []
for name, base, shift in (("LEFT ", 27, 24), ("RIGHT", 52, 0)):
    ev = list(events(arr("s_song_left" if name.startswith("L") else "s_song_right"), base, shift))
    tot = sum(d for _, d, _ in ev)
    print("== %s == 事件 %d，总时长 %.1fs，最高音范围 %.0f~%.0f Hz" %
          (name, len(ev), tot, min(m for m,_,_ in ev), max(m for m,_,_ in ev)))
    for lo, hi, lab in TIERS:
        c = sum(1 for m, d, _ in ev if lo <= m < hi); dd = sum(d for m, d, _ in ev if lo <= m < hi)
        print("   %-12s %5d 事件 (%4.1f%%)  时长 %6.1fs (%4.1f%%)" % (lab, c, 100.0*c/len(ev), dd, 100.0*dd/tot))
    pool += [(m, d) for m, d, _ in ev]
    print()
pool.sort(); cum=[]; s=0.0
for m,d in pool: s+=d; cum.append(s)
def q(p):
    t=s*p; i=bisect.bisect_left(cum,t); return pool[min(i,len(pool)-1)][0]
print("== 合并（时长加权分位数）==")
for p in (0.10,0.25,0.50,0.75,0.90,1.0):
    print("   %3d%% = %6.0f Hz" % (int(p*100), q(min(p,1.0))))
print("   建议 4 档分界（四分位）: %.0f / %.0f / %.0f Hz" % (q(0.25), q(0.50), q(0.75)))