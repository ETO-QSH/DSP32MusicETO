import json
L_JSON = r"D:\Documents\Documents\deepseek-harness\default-workspace\midi\out_left3\fuwa_events_rh0.json"
R_JSON = r"D:\Documents\Documents\deepseek-harness\default-workspace\midi\fuwa_events_rh0.json"
L_BASE, R_BASE = 27, 52          # 左手 MIDI 28..75 -> v 1..49 ; 右手 MIDI 53..100 -> v 1..48
OUT = r"C:\Users\ETO\workspace_ccstheia\DSP32Project\User\song_tables.h"

def unit(ms):
    return max(1, min(255, int(round(ms * 3.0 / 250.0))))

def pack(ev, base, vmax):
    words = []; prev_end = 0.0; folded = 0; total = 0; mx = 0
    for e in ev:
        st, en = e['start'], e['end']
        if st - prev_end > 0.020:
            words += [0, (unit((st - prev_end) * 1000.0) << 6)]
        v = [0, 0, 0, 0]; k = 0
        for p in e.get('raw', []):
            p = int(round(p)); total += 1
            x = p - base + 1                 # 1..49（0 保留为"无音"）
            if x < 1:  x = 1;  folded += 1
            if x > vmax: x = vmax; folded += 1
            if x > mx: mx = x
            if k < 4: v[k] = x; k += 1
        u = unit(max(40.0, (en - st) * 1000.0))
        words += [v[0] | (v[1] << 6) | (v[2] << 12), v[3] | (u << 6)]
        prev_end = en
    words += [0, 0]
    return words, total, folded, mx

def emit(f, name, words, base):
    f.write("/* %s : v = MIDI - %d + 1 (1..49), %d words = %d bytes */\n" % (name, base, len(words), len(words)*2))
    f.write("static const U16 %s[] = {\n" % name)
    for i in range(0, len(words), 8):
        f.write("    " + ", ".join("%dU" % w for w in words[i:i+8]) + ",\n")
    f.write("};\n\n")

L = json.load(open(L_JSON, encoding='utf-8')); R = json.load(open(R_JSON, encoding='utf-8'))
lw, lt, lf, lmx = pack(L, L_BASE, 49)
rw, rt, rf, rmx = pack(R, R_BASE, 48)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write("/* 自动生成 by midi_pack_2hands.py —— 双手曲表【6bit 音高版，无折叠】\n")
    f.write("   word0 = v0[5:0] | v1[11:6] | v2[17:12]      (音高各 6bit)\n")
    f.write("   word1 = v3[5:0] | 时值[13:6]                 (时值单位 1/16 音符 = 250/3 ms)\n")
    f.write("   v = MIDI - base + 1，0 = 无音；表尾 (0,0) */\n")
    f.write("#ifndef SONG_TABLES_H_\n#define SONG_TABLES_H_\n\n")
    f.write("#define SONG_L_BASE   %dU\n#define SONG_R_BASE   %dU\n#define SONG_PITCH_MAX 49U\n\n" % (L_BASE, R_BASE))
    emit(f, "s_song_left", lw, L_BASE); emit(f, "s_song_right", rw, R_BASE)
    f.write("#endif\n")
print("LEFT  notes=%d clamped=%d max_v=%d words=%d" % (lt, lf, lmx, len(lw)))
print("RIGHT notes=%d clamped=%d max_v=%d words=%d" % (rt, rf, rmx, len(rw)))