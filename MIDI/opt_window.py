import json, collections
CASES = [("LEFT ", r"D:\Documents\Documents\deepseek-harness\default-workspace\midi\out_left3\fuwa_events_rh0.json"),
         ("RIGHT", r"D:\Documents\Documents\deepseek-harness\default-workspace\midi\fuwa_events_rh0.json")]
def fold(p, lo, hi):
    n = 0
    while p < lo: p += 12; n -= 1
    while p > hi: p -= 12; n += 1
    return p, n
def nm(m):  # MIDI -> note name
    names = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
    return "%s%d" % (names[m % 12], m//12 - 1)
for name, path in CASES:
    ev = json.load(open(path, encoding='utf-8'))
    pitches = [p for e in ev for p in e.get('raw', [])]
    hist = collections.Counter(pitches)
    print("== %s ==  notes=%d  raw range MIDI %d..%d (%s..%s)" %
          (name, len(pitches), min(pitches), max(pitches), nm(min(pitches)), nm(max(pitches))))
    best = None
    for lo in range(min(pitches) - 12, max(pitches) + 1):
        hi = lo + 23
        cnt = sum(v for p, v in hist.items() if lo <= p <= hi)
        foldcnt = len(pitches) - cnt
        if best is None or cnt > best[1]:
            best = (lo, cnt, foldcnt)
    lo, cnt, foldcnt = best
    print("   best window MIDI %d..%d (%s..%s): in-window %d, folded %d (%.1f%%)" %
          (lo, lo+23, nm(lo), nm(lo+23), cnt, foldcnt, 100.0*foldcnt/len(pitches)))
    dist = collections.Counter()
    bad = 0
    for p in pitches:
        q, n = fold(p, lo, lo+23)
        dist[n] += 1
        if not (lo <= q <= lo+23): bad += 1
    print("   fold-octave distribution: %s   out-of-window after fold = %d" % (dict(sorted(dist.items())), bad))
    # contour flips
    prev_p = prev_q = None; pairs = flips = 0
    for e in ev:
        for p in e.get('raw', []):
            q, _ = fold(p, lo, lo+23)
            if prev_p is not None:
                pairs += 1
                if (p - prev_p) * (q - prev_q) < 0: flips += 1
            prev_p, prev_q = p, q
    print("   contour flips: %d/%d (%.1f%%)" % (flips, pairs, 100.0*flips/pairs if pairs else 0))
    print()