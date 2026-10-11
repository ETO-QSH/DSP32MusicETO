#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
midi_to_chords_rh.py —— 右手谱版和弦提取（用户思路：只取右手，跨度小 → 和弦更好）

关键差别（对比 midi_to_chords.py 的全谱版）：
    全谱版：每个音各自按八度折叠进 1.0~4.0kHz
            → 和弦内部音程被打乱、和弦被"压扁"、常退化成 2 个音级
    右手版：① 只取右手音区的音（默认 C3 以上）
            ② 整体块移调（找一个八度位移），使**尽量多的音自己就落在**
               1.0~4.0kHz 内 —— 只有少数越界音才折叠
            → 和弦内部音程原样保留，和弦性质/转位都尽量忠实

用法:
  python midi_to_chords_rh.py <in.mid> <outdir> [split] [barA] [barB]
    split: 右手分界音高（MIDI 号，默认 48 = C3；越大越"纯右手"）
"""
import sys
import os
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from midi_analyze import (parse_smf, collect_notes, tempo_map, tick_to_sec,
                          pitch_name, freq_of)
from midi_to_chords import (build_events, chord_name, render, BAR, MAX_VOICES,
                            BAND_LO, BAND_HI)

DEFAULT_SPLIT = 48          # C3：右手分界


def pick_shift(all_pitches, lo=BAND_LO, hi=BAND_HI):
    """在整八度位移里挑一个，使越界(需折叠)的音最少；并列时取跨度更小的"""
    best = None
    for shift in range(-12, 49, 12):
        out = sum(1 for p in all_pitches if freq_of(p + shift) < lo or freq_of(p + shift) > hi)
        span = (max(all_pitches) - min(all_pitches))
        key = (out, span)
        if best is None or key < best[0]:
            best = (key, shift, out)
    return best[1], best[2]


def main():
    src, outdir = sys.argv[1], sys.argv[2]
    split = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3].lstrip('-').isdigit() else DEFAULT_SPLIT
    bar_a = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4].isdigit() else 0
    bar_b = int(sys.argv[5]) if len(sys.argv) > 5 and sys.argv[5].isdigit() else 0
    os.makedirs(outdir, exist_ok=True)

    fmt, div, tracks = parse_smf(src)
    all_ev = [e for tr in tracks for e in tr]
    tmap = tempo_map(all_ev, div)
    notes = collect_notes(tracks[1])
    events = build_events(notes, tmap, div)
    if bar_a or bar_b:
        a = (bar_a - 1) * BAR if bar_a else 0
        b = bar_b * BAR if bar_b else 10 ** 9
        events = [e for e in events if a <= e['start'] < b]

    # ① 只留右手音区的音；整簇都在分界以下的（纯左手）直接不要
    kept = 0
    dropped = 0
    for e in events:
        rh = [p for p in e['raw'] if p >= split]
        if not rh:
            e['skip'] = True
            dropped += 1
            continue
        e['raw'] = rh
        kept += 1
    events = [e for e in events if not e.get('skip')]

    # ② 整体块移调（最小折叠）
    all_p = [p for e in events for p in e['raw']]
    shift, outliers = pick_shift(all_p)
    lo = min(all_p) + shift
    hi = max(all_p) + shift

    # ③ 每个和弦"整块"移调（保持和弦内部音程关系），只对跨度过大的和弦逐音折叠
    played = []
    prev_center = None
    for e in events:
        ps = sorted(p + shift for p in e['raw'])
        best = None
        for k in range(-4, 5):                      # 试整八度位移
            qs = [p + 12 * k for p in ps]
            out = sum(1 for q in qs if freq_of(q) < BAND_LO or freq_of(q) > BAND_HI)
            center = sum(qs) / len(qs)
            smooth = 0.0 if prev_center is None else abs(center - prev_center)
            key = (out, smooth)                     # 先保证落带内，再求相邻和弦平滑
            if best is None or key < best[0]:
                best = (key, qs)
        qs = best[1]
        prev_center = sum(qs) / len(qs)

        folded = []
        for p in qs:                                # 只剩宽和弦的越界音才逐音折
            while freq_of(p) < BAND_LO:
                p += 12
            while freq_of(p) > BAND_HI:
                p -= 12
            if p not in folded:
                folded.append(p)
        folded.sort()
        if len(folded) > MAX_VOICES:                  # 留最低+最高+中间
            keep = {folded[0], folded[-1]}
            mid = folded[1:-1]
            step = max(1, len(mid) // (MAX_VOICES - 2))
            keep.update(mid[::step][:MAX_VOICES - 2])
            folded = sorted(keep)
        e['pitches'] = folded
        e['play'] = [int(round(freq_of(p))) for p in folded]
        played.extend(folded)

    # ---------------- 输出
    tag = '_rh%d' % split
    if bar_a or bar_b:
        tag += '_%d-%d' % (bar_a, bar_b)

    hdr = ['/* 自动生成（右手谱版）：fuwa%s' % tag,
           '   右手分界 %s(MIDI %d)，块移调 %+d 半音，越界音 %d 个才折叠' %
           (pitch_name(split), split, shift, outliers),
           '   实际发声 %s..%s = %d..%d Hz（已折叠进 %.0f~%.0f），和弦内部音程尽量原样保留 */' %
           (pitch_name(min(played)), pitch_name(max(played)),
            int(freq_of(min(played))), int(freq_of(max(played))), BAND_LO, BAND_HI),
           '#include "audio.h"    /* CHORD_NOTE 类型在 audio.h 中定义 */',
           '',
           'static const CHORD_NOTE s_song_fuwa%s[] = {' % tag]
    prev_end_s = 0.0
    for e in events:
        t0 = tick_to_sec(e['start'], tmap, div)
        t1 = tick_to_sec(e['end'], tmap, div)
        gap = int(round((t0 - prev_end_s) * 1000))
        if gap > 20:                                  # 空隙 → 静音事件（{0,0,0,0},ms）
            hdr.append('    {{0U,0U,0U,0U},%5dU},' % gap)
        f = list(e['play']) + [0] * (MAX_VOICES - len(e['play']))
        ms = max(40, int(round((t1 - t0) * 1000)))
        hdr.append('    {{%5dU,%5dU,%5dU,%5dU},%5dU},' % (f[0], f[1], f[2], f[3], ms))
        prev_end_s = t1
    hdr.append('    {{0U,0U,0U,0U},0U}   /* 结束 */')
    hdr.append('};')
    hdr.append('/* %d 个和弦事件 ≈ %d 字节 */' % (len(events), (len(events) + 1) * 10))
    open(os.path.join(outdir, 'fuwa_chords%s.h' % tag), 'w', encoding='utf-8').write('\n'.join(hdr) + '\n')

    with open(os.path.join(outdir, 'fuwa_chord_score%s.md' % tag), 'w', encoding='utf-8') as f:
        f.write('# ふわふわ時間 — 右手谱和弦版（分界 %s，块移调 %+d，发声 %s..%s）\n\n'
                % (pitch_name(split), shift, pitch_name(lo), pitch_name(hi)))
        f.write('| 小节 | 拍 | 和弦 | 原始音(右手) | 发声Hz | 时值ms |\n|---|---|---|---|---|---|\n')
        for e in events:
            ms = int((tick_to_sec(e['end'], tmap, div) - tick_to_sec(e['start'], tmap, div)) * 1000)
            f.write('| %d | %.2f | %s | %s | %s | %d |\n' %
                    (e['start'] // BAR + 1, (e['start'] % BAR) / 960 + 1,
                     chord_name(e['pitches']),
                     ' '.join(pitch_name(p) for p in e['raw']),
                     ' '.join(str(x) for x in e['play']), ms))

    render(os.path.join(outdir, 'fuwa_arpeggio%s.wav' % tag), events, tmap, div, 'arp')
    render(os.path.join(outdir, 'fuwa_stack%s.wav' % tag), events, tmap, div, 'stack')

    # 额外导出时间线 JSON，供合成器预览/固件参考
    import json
    with open(os.path.join(outdir, 'fuwa_events%s.json' % tag), 'w', encoding='utf-8') as jf:
        json.dump([{'start': round(tick_to_sec(e['start'], tmap, div), 4),
                    'end': round(tick_to_sec(e['end'], tmap, div), 4),
                    'raw': e['raw'], 'play': e['play'], 'pitches': e['pitches']}
                   for e in events], jf, ensure_ascii=False)

    vh = defaultdict(int)
    for e in events:
        vh[len(e['play'])] += 1
    print('右手分界 %s(MIDI %d)' % (pitch_name(split), split))
    print('和弦事件 %d 个（丢弃纯左手事件 %d 个）；块移调 %+d 半音，仅 %d 个音需折叠'
          % (len(events), dropped, shift, outliers))
    print('实际发声范围 %s..%s = %d..%d Hz（已折叠进 %.0f~%.0f）'
          % (pitch_name(min(played)), pitch_name(max(played)),
             int(freq_of(min(played))), int(freq_of(max(played))), BAND_LO, BAND_HI))
    print('声部数分布 %s；时长 %.1f 秒；表 ≈%d 字节'
          % (dict(sorted(vh.items())), tick_to_sec(events[-1]['end'], tmap, div), (len(events) + 1) * 10))


if __name__ == '__main__':
    main()
