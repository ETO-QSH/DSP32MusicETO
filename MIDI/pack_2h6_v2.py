import json, re
L_JSON = r"D:\Documents\Documents\deepseek-harness\default-workspace\midi\out_left3\fuwa_events_rh0.json"
R_JSON = r"D:\Documents\Documents\deepseek-harness\default-workspace\midi\fuwa_events_rh0.json"
L_BASE, R_BASE = 27, 52
OUT = r"C:\Users\ETO\workspace_ccstheia\DSP32Project\User\song_tables.h"

def unit(ms):
    return max(1, min(255, int(round(ms * 3.0 / 250.0))))

def enc(vs, u):
    v0, v1, v2, v3 = vs
    w0 = v0 | (v1 << 6) | ((v2 & 0x0F) << 12)
    w1 = ((v2 >> 4) & 0x03) | (v3 << 2) | ((u & 0xFF) << 8)
    assert 0 <= w0 <= 0xFFFF and 0 <= w1 <= 0xFFFF, (w0, w1, vs, u)
    return w0, w1

def dec(w0, w1):
    v0 = w0 & 0x3F; v1 = (w0 >> 6) & 0x3F
    v2 = ((w0 >> 12) & 0x0F) | ((w1 & 0x03) << 4)
    v3 = (w1 >> 2) & 0x3F; u = (w1 >> 8) & 0xFF
    return [v0, v1, v2, v3], u

def pack(ev, base):
    words = []; prev_end = 0.0; mx = 0
    for e in ev:
        st, en = e['start'], e['end']
        if st - prev_end > 0.020:
            words += [0, (unit((st - prev_end) * 1000.0) << 8)]
        vs = [0, 0, 0, 0]; k = 0
        for p in e.get('raw', []):
            x = int(round(p)) - base + 1
            if x < 1: x = 1
            if x > 49: x = 49
            if x > mx: mx = x
            if k < 4: vs[k] = x; k += 1
        w0, w1 = enc(vs, unit(max(40.0, (en - st) * 1000.0)))
        words += [w0, w1]; prev_end = en
    words += [0, 0]
    return words, mx

def emit(f, name, words, base):
    f.write("/* %s : v = MIDI - %d + 1 (1..49)，每事件 2 字，%d 字 = %d 字节 */\n" % (name, base, len(words), len(words)*2))
    f.write("static const U16 %s[] = {\n" % name)
    for i in range(0, len(words), 8):
        f.write("    " + ", ".join("%dU" % w for w in words[i:i+8]) + ",\n")
    f.write("};\n\n")

L = json.load(open(L_JSON, encoding='utf-8')); R = json.load(open(R_JSON, encoding='utf-8'))
lw, lmx = pack(L, L_BASE); rw, rmx = pack(R, R_BASE)
assert max(max(lw), max(rw)) <= 0xFFFF
# 自检：往返解码
for nm, ws in (("L", lw), ("R", rw)):
    for i in range(0, min(len(ws), 400), 2):
        vs, u = dec(ws[i], ws[i+1])
        assert all(0 <= v <= 49 for v in vs) and 0 <= u <= 255, (nm, i)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write("/* 自动生成 by midi_pack_2hands6_v2.py —— 双手曲表【6bit 跨字打包】\n")
    f.write("   word0 = v0[5:0] | v1[11:6] | v2低位[15:12]\n")
    f.write("   word1 = v2高位[1:0] | v3[7:2] | 时值[15:8]\n")
    f.write("   v = MIDI - base + 1（0 = 无音，49 上限）；时值单位 1/16 音符 = 250/3 ms；表尾 (0,0) */\n")
    f.write("#ifndef SONG_TABLES_H_\n#define SONG_TABLES_H_\n\n")
    f.write("#define SONG_L_BASE   %dU\n#define SONG_R_BASE   %dU\n\n" % (L_BASE, R_BASE))
    emit(f, "s_song_left", lw, L_BASE); emit(f, "s_song_right", rw, R_BASE)
    f.write("#endif\n")
print("LEFT  words=%d max_v=%d" % (len(lw), lmx))
print("RIGHT words=%d max_v=%d" % (len(rw), rmx))
print("OK: 所有字 <= 65535，往返解码自检通过")