# -*- coding: utf-8 -*-
"""
midi_pack_song.py —— 把和弦事件流打包成 U16 表（每事件 2 个字 = 4 字节）

打包格式
    word0 : 声部 0/1/2 的音高，各占 5 bit（bit0-4 / bit5-9 / bit10-14）
              值 = MIDI 音符号 - 83   （1..24 对应 C6..B7，即两个八度）
              0  = 该声部无音
    word1 : bit0-7  = 时值，单位 = 1/16 音符 @180BPM = 250/3 ms ≈ 83.333 ms
                        （实测曲中最大 30 个单位，1 字节够）
            bit8-12 = 声部 3 的音高（同样 5 bit）
    表尾  : (0, 0) 终止

为什么这样打包
    * C28x 是字寻址，char/U8 仍占 16 位 → 缩体积必须手工位打包，不能靠减字节字段
    * 折叠后实际发声范围是 C6..B7（两个八度 = 24 个音）→ 5 bit/音
      4 个音共 20 bit，塞不进一个字 → 3 个进 word0，第 4 个与时值共用 word1
    * 每事件 2 字 = 4 字节（原格式 5 字 = 10 字节）

用法:  python midi_pack_song.py <events.json> <out_packed.h> [ref_chords.h]
       给了 ref_chords.h 就逐条比对（频率按 12 音表还原，时值容差 2ms）
"""
import json
import math
import re
import sys

UNIT_NUM = 250          # ms = units * UNIT_NUM / UNIT_DEN
UNIT_DEN = 3            # 250/3 = 83.333... ms  = 1/16 音符 @180BPM
MAX_VOICES = 4
PITCH_BASE = 83         # 值 = MIDI - 83（C6 = 84 → 1）


def note_hz(midi):
    """MIDI 音符号 → 频率(Hz)，与固件音高表一致"""
    return 440.0 * (2.0 ** ((midi - 69) / 12.0))


def unit_of(ms):
    return int(round(ms * UNIT_DEN / UNIT_NUM))


def main():
    src, out_h = sys.argv[1], sys.argv[2]
    ref_h = sys.argv[3] if len(sys.argv) > 3 else None

    ev = [e for e in json.load(open(src, encoding='utf-8')) if not e.get('skip')]
    ev.sort(key=lambda e: e['start'])

    words = []
    events = []                 # [( pitches, ms_restored ), ...] 供等价性校验
    prev_end = 0.0
    max_unit = 0
    max_quant_err = 0.0

    for i, e in enumerate(ev):
        end = e['end']
        if i + 1 < len(ev):
            end = min(end, ev[i + 1]['start'])

        # ---- 空隙 → 静音事件 ----
        gap = e['start'] - prev_end
        if gap > 0.020:
            u = unit_of(gap * 1000.0)
            if u > 0:
                words.append(0)
                words.append(u)
                events.append(([], round(u * UNIT_NUM / UNIT_DEN)))
                max_unit = max(max_unit, u)

        # 事件里的发声音：JSON 的 play 字段存的是 Hz，反算回 MIDI 音符号
        pitches = []
        for raw in list(e['play'])[:MAX_VOICES]:
            nn = float(raw)
            if nn > 127.0:                      # 明显不是音符号 → 当频率处理
                nn = 69.0 + 12.0 * math.log2(nn / 440.0)
            pitches.append(int(round(nn)))
        v = [0, 0, 0, 0]
        for k, n in enumerate(pitches):
            x = int(n) - PITCH_BASE
            if not (1 <= x <= 24):
                raise SystemExit('pitch out of folded band: MIDI %d' % int(n))
            v[k] = x
        w0 = v[0] | (v[1] << 5) | (v[2] << 10)

        ms = max(40.0, (end - e['start']) * 1000.0)
        u = max(1, unit_of(ms))
        if u > 255:
            raise SystemExit('duration %d units exceeds 1 byte' % u)
        w1 = (v[3] << 8) | (u & 0xFF)
        # 回读校验（确保位打包可逆）
        assert (w0 & 31) == v[0] and ((w0 >> 5) & 31) == v[1] and ((w0 >> 10) & 31) == v[2]
        assert ((w1 >> 8) & 31) == v[3] and (w1 & 0xFF) == u
        words.append(w0)
        words.append(w1)
        events.append((pitches, round(u * UNIT_NUM / UNIT_DEN)))
        max_unit = max(max_unit, u)
        max_quant_err = max(max_quant_err, abs(u * UNIT_NUM / UNIT_DEN - ms))
        prev_end = end

    words.append(0)             # 终止
    words.append(0)

    # ---- 输出 C 头文件（每行 16 个字）----
    lines = []
    lines.append('/* 自动生成 by midi_pack_song.py —— 打包曲表（每事件 2 字 = 4 字节）')
    lines.append('   word0 = 4 声部音高，各 4bit（值 = MIDI-83，1..12 → C6..B7，0 = 无音）')
    lines.append('   word1 = 时值，单位 1/16 音符 = %d/%d ms ≈ %.3f ms；表尾 (0,0)'
                 % (UNIT_NUM, UNIT_DEN, UNIT_NUM / UNIT_DEN))
    lines.append('   共 %d 个事件，最大时值 %d 单位，最大量化误差 %.2f ms */'
                 % (len(events), max_unit, max_quant_err))
    lines.append('#ifndef SONG_PACKED_H_')
    lines.append('#define SONG_PACKED_H_')
    lines.append('')
    lines.append('#define SONG_UNIT_NUM   %dU' % UNIT_NUM)
    lines.append('#define SONG_UNIT_DEN   %dU' % UNIT_DEN)
    lines.append('#define SONG_EVENT_NUM  %dU' % len(events))
    lines.append('')
    lines.append('static const U16 s_song_packed[] = {')
    for i in range(0, len(words), 8):
        chunk = words[i:i + 8]
        lines.append('    ' + ' '.join('%5dU,' % w for w in chunk))
    lines.append('};')
    lines.append('')
    lines.append('#endif /* SONG_PACKED_H_ */')
    open(out_h, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')

    used_words = 2 * (len(events) + 1)
    print('打包完成: %s' % out_h)
    print('  事件 %d 个（含空隙静音事件），表 %d 字 = %d 字节（原格式 5 字/事件 ≈ %d 字节）'
          % (len(events), used_words, used_words * 2, (len(events) + 1) * 10))
    print('  最大时值 %d 单位（1 字节上限 255），最大量化误差 %.2f ms' % (max_unit, max_quant_err))

    # ---- 等价性校验 ----
    if ref_h:
        txt = open(ref_h, encoding='utf-8').read()
        ref = [([int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))],
                int(m.group(5)))
               for m in re.finditer(
                   r'\{\{\s*(\d+)U,\s*(\d+)U,\s*(\d+)U,\s*(\d+)U\},\s*(\d+)U\}', txt)]
        bad = 0
        n = min(len(ref), len(events))
        for i in range(n):
            rf, rms = ref[i]
            pf, pms = events[i]
            df = 0.0
            for k in range(MAX_VOICES):
                a = rf[k]
                b = note_hz(pf[k]) if k < len(pf) else 0.0
                df = max(df, abs(a - b))
            if df > 1.5 or abs(rms - pms) > 2:
                bad += 1
                if bad <= 5:
                    print('  差异 #%d: ref freq=%s ms=%d | packed freq=%s ms=%d'
                          % (i, rf, rms, [round(note_hz(x), 1) for x in pf], pms))
        print('  等价性: 参考 %d 条 / 打包 %d 条，比对 %d 条，不一致 %d 条'
              % (len(ref), len(events), n, bad))
        if len(ref) != len(events):
            print('  ★ 条数不同，请检查空隙事件规则')
        return 0 if bad == 0 else 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
