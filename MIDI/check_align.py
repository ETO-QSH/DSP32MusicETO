import json, struct, sys
def span(path, label):
    ev = json.load(open(path, encoding='utf-8'))
    st = min(e['start'] for e in ev); en = max(e['end'] for e in ev)
    print("  %-28s 事件%4d  start %.3f  end %.3f  跨度 %.1f s" % (label, len(ev), st, en, en-st))
    return st, en
print("=== 事件 JSON 的时间跨度 ===")
span(r"D:\Documents\Documents\deepseek-harness\default-workspace\midi\out_left3\fuwa_events_rh0.json", "LEFT  out_left3")
span(r"D:\Documents\Documents\deepseek-harness\default-workspace\midi\fuwa_events_rh0.json", "RIGHT fuwa_events_rh0")

def read_vlq(b,i):
    v=0
    while True:
        c=b[i]; i+=1; v=(v<<7)|(c&0x7F)
        if not (c&0x80): return v,i
def info(path):
    b=open(path,'rb').read()
    ntrk=struct.unpack('>H',b[10:12])[0]; div=struct.unpack('>H',b[12:14])[0]
    i=14; last=0; first=None; notes=0
    for _ in range(ntrk):
        ln=struct.unpack('>I',b[i+4:i+8])[0]; i+=8; end=i+ln; t=0; run=None
        while i<end:
            d,i=read_vlq(b,i); t+=d; st=b[i]
            if st&0x80: run=st; i+=1
            else: st=run
            if st is None: break
            if st==0xFF:
                typ=b[i]; i+=1; l,i=read_vlq(b,i); i+=l
            elif st in (0xF0,0xF7):
                l,i=read_vlq(b,i); i+=l
            elif st&0xF0 in (0x80,0x90,0xA0,0xB0,0xE0):
                p1=b[i]; p2=b[i+1]; i+=2
                if (st&0xF0)==0x90 and p2>0:
                    notes+=1
                    if first is None: first=t
            elif st&0xF0 in (0xC0,0xD0): i+=1
            else: break
            last=max(last,t)
        i=end
    name=path.split('\\')[-1]
    print("  %-26s 分辨率%5d  首音 tick %-7s 末事件 tick %-8d  ≈ %.1f s   音数 %d" %
          (name, div, first, last, last/div, notes))
print("=== MIDI 文件本身 ===")
for f in ("ふわふわ時間.mid","ふわふわ時間-left.mid","ふわふわ時間-right.mid"):
    info(r"D:\Desktop\Desktop\%s" % f)