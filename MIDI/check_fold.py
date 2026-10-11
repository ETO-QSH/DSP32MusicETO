import json
CASES = [("左手", r"D:\Documents\Documents\deepseek-harness\default-workspace\midi\out_left3\fuwa_events_rh0.json", 48, 71, "C3..B4"),
         ("右手", r"D:\Documents\Documents\deepseek-harness\default-workspace\midi\fuwa_events_rh0.json",          84, 107, "C6..B7")]
def fold(p, lo, hi):
    n = 0
    while p < lo: p += 12; n -= 1
    while p > hi: p -= 12; n += 1
    return p, n
for name, path, lo, hi, wn in CASES:
    ev = json.load(open(path, encoding='utf-8'))
    tot = inw = 0; dist = {}; bad = 0; folded = 0
    prev_p = prev_q = None; pairs = flips = 0
    for e in ev:
        for p in e.get('raw', []):
            tot += 1
            q, n = fold(p, lo, hi)
            if not (lo <= q <= hi): bad += 1
            if n == 0: inw += 1
            else: folded += 1
            dist[n] = dist.get(n, 0) + 1
            if prev_p is not None:
                pairs += 1
                if (p - prev_p) * (q - prev_q) < 0: flips += 1
            prev_p, prev_q = p, q
    print("== %s 窗口 %s (MIDI %d..%d) ==" % (name, wn, lo, hi))
    print("   音符总数 %d ；窗内无需折 %d ；需折 %d" % (tot, inw, folded))
    print("   折八度分布（负=折高，正=折低）: %s" % dict(sorted(dist.items())))
    print("   折叠后越界个数 = %d  %s" % (bad, "✓ 全部在窗内" if bad == 0 else "✗ 有越界"))
    print("   相邻音走向被折反的比例 = %d/%d (%.1f%%)" % (flips, pairs, 100.0*flips/pairs if pairs else 0))
    print()