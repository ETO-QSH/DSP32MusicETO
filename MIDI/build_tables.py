#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_tables.py -- single-file pipeline: two MIDI files -> song_tables.h

    python build_tables.py            # uses the .mid files next to this script

Pipeline: parse SMF -> collect notes -> build chord events -> 6-bit cross-word
packing -> U16 table header. All paths are relative to this script, so the
MIDI/ folder is self-contained.

Packed layout (2 words per event):
    word0 = v0[5:0] | v1[11:6] | v2_low[15:12]
    word1 = v2_high[1:0] | v3[7:2] | duration[15:8]
    v = MIDI - base + 1 (0 = none, max 49); duration unit = 1/16 note = 250/3 ms
"""


import io
import os
import struct

from collections import defaultdict


HERE = os.path.dirname(os.path.abspath(__file__))
L_MID = os.path.join(HERE, 'ふわふわ時間-left.mid')
R_MID = os.path.join(HERE, 'ふわふわ時間-right.mid')
OUT = os.path.join(HERE, 'song_tables.h')
L_BASE, R_BASE = 27, 52

# ===================== MIDI parsing (from midi_analyze.py) =====================


def read_vlq(d, i):
    """读取可变长度数值（MIDI 的时间增量）"""
    v = 0
    while True:
        b = d[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not (b & 0x80):
            return v, i


def parse_track(d):
    """解析一条 MTrk，返回事件列表 [{tick, type, ...}]"""
    events = []
    i = 0
    tick = 0
    running = None  # running status

    while i < len(d):
        delta, i = read_vlq(d, i)
        tick += delta

        if i >= len(d):
            break

        status = d[i]

        if status == 0xFF:  # meta 事件
            i += 1
            mtype = d[i]
            i += 1
            mlen, i = read_vlq(d, i)
            mdata = d[i:i + mlen]
            i += mlen
            events.append({'tick': tick, 'type': 'meta', 'meta': mtype, 'data': mdata})
            running = None
            continue

        if status in (0xF0, 0xF7):  # sysex
            i += 1
            slen, i = read_vlq(d, i)
            i += slen
            running = None
            continue

        if status & 0x80:  # 有状态字节
            running = status
            i += 1
        else:  # running status
            status = running
            if status is None:
                break

        hi = status & 0xF0
        ch = status & 0x0F

        if hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
            d1, d2 = d[i], d[i + 1]
            i += 2
            events.append({'tick': tick, 'type': hi, 'ch': ch, 'd1': d1, 'd2': d2})
        elif hi in (0xC0, 0xD0):
            d1 = d[i]
            i += 1
            events.append({'tick': tick, 'type': hi, 'ch': ch, 'd1': d1, 'd2': 0})
        else:
            break

    return events


def parse_smf(path):
    data = open(path, 'rb').read()
    if data[:4] != b'MThd':
        raise ValueError('不是标准 MIDI 文件')

    hlen = struct.unpack('>I', data[4:8])[0]
    fmt, ntrk, div = struct.unpack('>HHH', data[8:14])
    pos = 8 + hlen
    tracks = []

    for _ in range(ntrk):
        if data[pos:pos + 4] != b'MTrk':
            break

        tlen = struct.unpack('>I', data[pos + 4:pos + 8])[0]
        tracks.append(parse_track(data[pos + 8:pos + 8 + tlen]))
        pos += 8 + tlen

    return fmt, div, tracks


NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']


def pitch_name(p):
    return '%s%d' % (NOTE_NAMES[p % 12], p // 12 - 1)


def freq_of(p):
    """MIDI 音高 → 频率(Hz)，A4=440"""
    return 440.0 * (2.0 ** ((p - 69) / 12.0))


def collect_notes(events):
    """把 note on/off 配对成音符 [{start, end, pitch, vel, ch}]"""
    notes = []
    pending = defaultdict(list)  # (ch, pitch) -> [(tick, vel)]

    for e in events:
        if e['type'] == 0x90 and e['d2'] > 0:  # note on
            pending[(e['ch'], e['d1'])].append((e['tick'], e['d2']))
        elif e['type'] == 0x80 or (e['type'] == 0x90 and e['d2'] == 0):  # note off
            key = (e['ch'], e['d1'])
            if pending[key]:
                st, vel = pending[key].pop(0)
                notes.append({'start': st, 'end': e['tick'], 'pitch': e['d1'], 'vel': vel, 'ch': e['ch']})

    notes.sort(key=lambda n: (n['start'], n['pitch']))
    return notes


def tempo_map(events, div):
    """返回 [(tick, us_per_quarter)]。
    注意：只有在文件里完全没有 tempo 事件时才补默认 120BPM；
    否则补的默认值会排在真实事件前面，导致时长算错（曾经踩过）。
    """
    t = []

    for e in events:
        if e['type'] == 'meta' and e['meta'] == 0x51 and len(e['data']) == 3:
            us = (e['data'][0] << 16) | (e['data'][1] << 8) | e['data'][2]
            t.append((e['tick'], us))

    if not t:
        t = [(0, 500000)]  # 默认 120BPM

    t.sort(key=lambda x: x[0])
    return t


def tick_to_sec(tick, tmap, div):
    """按 tempo map 把 tick 转成秒"""
    sec = 0.0
    last_tick, last_us = tmap[0]

    for tk, us in tmap[1:]:
        if tick <= tk:
            break
        sec += (tk - last_tick) * last_us / 1e6 / div
        last_tick, last_us = tk, us

    sec += (tick - last_tick) * last_us / 1e6 / div
    return sec


# ================== chord events (from midi_to_chords.py) =====================

QUANT = 240  # 1/16
CLUSTER_TOL = 40
BAR = 960 * 4
BAND_LO, BAND_HI = 1000.0, 4000.0  # 蜂鸣器工作频段
MAX_VOICES = 4

PC = {'C': 0, 'C#': 1, 'D': 2, 'D#': 3, 'E': 4, 'F': 5, 'F#': 6, 'G': 7, 'G#': 8, 'A': 9, 'A#': 10, 'B': 11}

TEMPLATES = [
    ((0, 4, 7), ''), ((0, 3, 7), 'm'), ((0, 3, 6), 'dim'), ((0, 4, 8), 'aug'),
    ((0, 5, 7), 'sus4'), ((0, 2, 7), 'sus2'),
    ((0, 4, 7, 11), 'M7'), ((0, 4, 7, 10), '7'), ((0, 3, 7, 10), 'm7'),
    ((0, 3, 6, 10), 'm7b5'), ((0, 3, 6, 9), 'dim7'), ((0, 4, 7, 9), '6'),
    ((0, 3, 7, 9), 'm6'),
]

NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
IVNAME = {0: '8ve', 1: 'm2', 2: '2nd', 3: 'm3', 4: '3rd', 5: '4th', 6: 'tri', 7: '5th', 8: 'm6', 9: '6th', 10: 'm7', 11: '7th'}


def chord_name(pitches):
    """按音级集合猜和弦名。
    注意：八度折叠后，同一个原始和弦常被压成 2 个音级（例如 E+B = 纯五度），
    所以 1 音级/2 音级要单独命名，否则会显示 '?'。
    """
    pcs = sorted(set(p % 12 for p in pitches))

    if not pcs:
        return '-'

    if len(pcs) == 1:
        return NAMES[pcs[0]] + '8ve'

    if len(pcs) == 2:
        return NAMES[pcs[0]] + IVNAME[(pcs[1] - pcs[0]) % 12]

    root = pcs[0]
    rel = tuple(sorted((p - root) % 12 for p in pcs))
    best = None

    for tmpl, suf in TEMPLATES:
        if all(x in rel for x in tmpl):
            score = len(tmpl) - (len(rel) - len(tmpl))
            if best is None or score > best[0]:
                best = (score, suf)

    return NAMES[root] + (best[1] if best else '(簇)') + ('+%d' % (len(pcs) - 4) if len(pcs) > 4 else '')


def cluster_onsets(notes, tol=CLUSTER_TOL):
    notes = sorted(notes, key=lambda n: n['start'])
    out, cur = [], []

    for n in notes:
        if cur and n['start'] - cur[0]['start'] > tol:
            out.append(cur)
            cur = []
        cur.append(n)

    if cur:
        out.append(cur)

    return out


def fold_into_band(p, lo=BAND_LO, hi=BAND_HI):
    while freq_of(p) < lo:
        p += 12
    while freq_of(p) > hi:
        p -= 12
    return p


def build_events(notes, tmap, div):
    """起音簇 → 和弦事件：{start, end, pitches(折叠后去重), raw(原始)}"""
    events = []

    for c in cluster_onsets(notes):
        # 去重：同音高只留一个；太短的装饰音丢掉
        seen = {}

        for n in c:
            if n['end'] - n['start'] < 120:
                continue
            seen[n['pitch']] = n

        ns = sorted(seen.values(), key=lambda n: n['pitch'])

        if not ns:
            continue

        start = min(n['start'] for n in ns)
        end = max(n['end'] for n in ns)
        events.append({'start': start, 'end': end, 'raw': [n['pitch'] for n in ns], 'vel': max(n['vel'] for n in ns)})

    # 量化
    for e in events:
        e['start'] = int(round(e['start'] / QUANT)) * QUANT
        e['end'] = max(int(round(e['end'] / QUANT)) * QUANT, e['start'] + QUANT)

    # 同一时刻合并
    bystart = {}

    for e in events:
        k = e['start']
        if k in bystart:
            bystart[k]['raw'] = sorted(set(bystart[k]['raw']) | set(e['raw']))
            bystart[k]['end'] = max(bystart[k]['end'], e['end'])
        else:
            bystart[k] = e

    out = sorted(bystart.values(), key=lambda e: e['start'])

    for i in range(len(out) - 1):  # 截断到下一个事件
        if out[i]['end'] > out[i + 1]['start']:
            out[i]['end'] = out[i + 1]['start']

    return out


# ===================== 6-bit packing (from pack_2h6_v2.py) =====================

def unit(ms):
    return max(1, min(255, int(round(ms * 3.0 / 250.0))))


def enc(vs, u):
    v0, v1, v2, v3 = vs
    w0 = v0 | (v1 << 6) | ((v2 & 0x0F) << 12)
    w1 = ((v2 >> 4) & 0x03) | (v3 << 2) | ((u & 0xFF) << 8)
    assert 0 <= w0 <= 0xFFFF and 0 <= w1 <= 0xFFFF, (w0, w1, vs, u)
    return w0, w1


def dec(w0, w1):
    v0 = w0 & 0x3F
    v1 = (w0 >> 6) & 0x3F
    v2 = ((w0 >> 12) & 0x0F) | ((w1 & 0x03) << 4)
    v3 = (w1 >> 2) & 0x3F
    u = (w1 >> 8) & 0xFF
    return [v0, v1, v2, v3], u


def pack(ev, base):
    words = []
    prev_end = 0.0
    mx = 0

    for e in ev:
        st, en = e['start'], e['end']

        if st - prev_end > 0.020:
            words += [0, (unit((st - prev_end) * 1000.0) << 8)]

        vs = [0, 0, 0, 0]
        k = 0

        for p in e.get('raw', []):
            x = int(round(p)) - base + 1
            if x < 1:
                x = 1
            if x > 49:
                x = 49
            if x > mx:
                mx = x
            if k < 4:
                vs[k] = x
                k += 1

        w0, w1 = enc(vs, unit(max(40.0, (en - st) * 1000.0)))
        words += [w0, w1]
        prev_end = en

    words += [0, 0]
    return words, mx


def emit(f, name, words, base):
    f.write("/* %s : v = MIDI - %d + 1 (1..49), 2 words per event, %d words = %d bytes */\n" % (name, base, len(words), len(words) * 2))
    f.write("static const U16 %s[] = {\n" % name)

    for i in range(0, len(words), 8):
        f.write("    " + ", ".join("%dU" % w for w in words[i:i + 8]) + ",\n")

    f.write("};\n\n")


def events_from_midi(path, split):
    """parse a single-hand MIDI and return the chord events (uses 'raw' pitches)"""
    fmt, div, tracks = parse_smf(path)
    all_ev = [e for tr in tracks for e in tr]
    tmap = tempo_map(all_ev, div)
    notes = collect_notes(tracks[1])
    ev = build_events(notes, tmap, div)
    out = []

    for e in ev:
        raw = [p for p in e['raw'] if p >= split]
        if raw:
            out.append({'start': tick_to_sec(e['start'], tmap, div), 'end': tick_to_sec(e['end'], tmap, div), 'raw': raw})

    return out


def main():
    L = events_from_midi(L_MID, 0)  # left hand: all notes
    R = events_from_midi(R_MID, 0)  # right hand: all notes
    lw, lmx = pack(L, L_BASE)
    rw, rmx = pack(R, R_BASE)

    assert max(max(lw), max(rw)) <= 0xFFFF

    for nm, ws in (("L", lw), ("R", rw)):  # round-trip self check
        for i in range(0, min(len(ws), 400), 2):
            vs, u = dec(ws[i], ws[i + 1])
            assert all(0 <= v <= 49 for v in vs) and 0 <= u <= 255, (nm, i)

    with io_open(OUT) as f:
        f.write("/* auto-generated by build_tables.py -- two-hand song table [6-bit cross-word]\n")
        f.write("   word0 = v0[5:0] | v1[11:6] | v2_low[15:12]\n")
        f.write("   word1 = v2_high[1:0] | v3[7:2] | duration[15:8]\n")
        f.write("   v = MIDI - base + 1 (0 = none, max 49); unit = 1/16 note = 250/3 ms; ends with (0,0) */\n")
        f.write("#ifndef SONG_TABLES_H_\n#define SONG_TABLES_H_\n\n")
        f.write("#define SONG_L_BASE   %dU\n#define SONG_R_BASE   %dU\n\n" % (L_BASE, R_BASE))
        emit(f, "s_song_left", lw, L_BASE)
        emit(f, "s_song_right", rw, R_BASE)
        f.write("#endif\n")

    print("LEFT  words=%d max_v=%d" % (len(lw), lmx))
    print("RIGHT words=%d max_v=%d" % (len(rw), rmx))
    print("OK: all words <= 65535, round-trip self check passed")


def io_open(path):
    return io.open(path, 'w', encoding='utf-8')


if __name__ == '__main__':
    main()
