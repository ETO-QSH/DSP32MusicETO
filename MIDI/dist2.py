import re, math
src=open(r"C:\Users\ETO\workspace_ccstheia\DSP32Project\User\song_tables.h",encoding='utf-8').read()
def arr(n):
    m=re.search(n+r'\[\]\s*=\s*\{(.*?)\};',src,re.S)
    return [int(x) for x in re.findall(r'(\d+)U',m.group(1))]
def hz(m): return 440.0*2.0**((m-69)/12.0)
def dec(w0,w1):
    v0=w0&0x3F; v1=(w0>>6)&0x3F
    v2=((w0>>12)&0x0F)|((w1&0x03)<<4)
    v3=(w1>>2)&0x3F; u=(w1>>8)&0xFF
    return [v0,v1,v2,v3],u
def events(words,base,shift):
    i=0
    while i+1<len(words):
        w0,w1=words[i],words[i+1]; i+=2
        if w0==0 and w1==0: break
        if w0==0: continue
        vs,u=dec(w0,w1)
        fs=[hz(base+v-1+shift) for v in vs if v]
        if fs: yield max(fs),fs,u*250.0/3.0
for name,key,base,shift in (("LEFT  (左手,+2 oct)",'s_song_left',27,24),
                            ("RIGHT (右手,原音域)",'s_song_right',52,0)):
    ev=list(events(arr(key),base,shift))
    tot=sum(u for _,_,u in ev)
    allf=[f for _,fs,_ in ev for f in fs]
    print("== %s ==" % name)
    print("   事件 %d，发声时长 %.1f s，音数 %d" % (len(ev),tot/1000.0,len(allf)))
    print("   频率范围：%.0f ~ %.0f Hz   （中位 %.0f Hz）" %
          (min(allf),max(allf),sorted(allf)[len(allf)//2]))
    oct_={}
    for _,fs,u in ev:
        for f in fs:
            o=int(math.floor(math.log2(f/16.3516)))
            oct_[o]=oct_.get(o,0.0)+u/len(fs)
    print("   八度分布（按发声时长加权）：")
    for o in sorted(oct_):
        if oct_[o]/tot < 0.005: continue
        print("      C%-2d  %6.1f~%6.1f Hz   %5.1f%%" % (o,16.3516*2**o,16.3516*2**(o+1),100.0*oct_[o]/tot))
    print("   每事件最高音 -> LED 档位：")
    for lo,hi,lab in ((0,440,"x1  <440Hz "),(440,660,"x2  440~660"),(660,880,"x3  660~880"),(880,1e9,"x4  >=880Hz")):
        d=sum(u for m,_,u in ev if lo<=m<hi)
        print("      %s  %5.1f%%" % (lab,100.0*d/tot))
    print()