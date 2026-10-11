import sys, struct
def read_vlq(b, i):
    v = 0
    while True:
        c = b[i]; i += 1
        v = (v << 7) | (c & 0x7F)
        if not (c & 0x80): return v, i
def notes(path):
    b = open(path,'rb').read()
    assert b[:4] == b'MThd'
    ntrk = struct.unpack('>H', b[10:12])[0]
    i = 14; lo, hi = 127, 0; cnt = 0
    for _ in range(ntrk):
        assert b[i:i+4] == b'MTrk'
        ln = struct.unpack('>I', b[i+4:i+8])[0]; i += 8
        end = i + ln; t = 0; run = None
        while i < end:
            d, i = read_vlq(b, i); t += d
            st = b[i]
            if st & 0x80: run = st; i += 1
            else: st = run
            if st is None: break
            if st == 0xFF:
                typ = b[i]; i += 1; l, i = read_vlq(b, i); i += l
            elif st in (0xF0, 0xF7):
                l, i = read_vlq(b, i); i += l
            elif st & 0xF0 in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                p1 = b[i]; p2 = b[i+1]; i += 2
                if (st & 0xF0) == 0x90 and p2 > 0:
                    lo = min(lo, p1); hi = max(hi, p1); cnt += 1
            elif st & 0xF0 in (0xC0, 0xD0):
                i += 1
            else: break
        i = end
    return lo, hi, cnt
for f in sys.argv[1:]:
    lo, hi, c = notes(f)
    print("  %-28s MIDI %d..%d   (共 %d 个音)" % (f.split('\\')[-1], lo, hi, c))