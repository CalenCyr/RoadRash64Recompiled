"""Offline MK64 US course-song renderer from the owner's cartridge image.

Reads the donor sequence/channel/layer programs and their VADPCM instruments;
this is not MIDI substitution. The emitted private bank contains bounded stereo
PCM and actual sequence loop points. RSP resampling/reverb are approximated on
the host. Unsupported commands fail rather than silently dropping a melody.
"""
from __future__ import annotations
import argparse, copy, hashlib, json, math, struct, wave
from pathlib import Path
import numpy as np
from extract_mk64_course_audio import Donor, decode_adpcm, u16, u32, f32, RATE

COURSES = {
 'mario_raceway':3, 'choco_mountain':5, 'bowsers_castle':9,
 'banshee_boardwalk':7, 'yoshi_valley':4, 'frappe_snowland':8,
 'koopa_troopa_beach':6, 'royal_raceway':3, 'luigi_raceway':3,
 'moo_moo_farm':4, 'toads_turnpike':21, 'kalimari_desert':10,
 'sherbet_land':8, 'rainbow_road':18, 'wario_stadium':3,
 'dks_jungle_parkway':19,
}
SEQUENCE_BANKS = {3:3,4:4,5:5,6:6,7:7,8:8,9:9,10:10,18:15,19:16,21:17}
def signed(x): return x-256 if x>=128 else x

class MusicDonor(Donor):
    def __init__(self, rom, seqid):
        super().__init__(rom)
        self.seqid=seqid; self.bankid=SEQUENCE_BANKS[seqid]
        off,size=struct.unpack_from('>II',self.rom,0xbc5f60+4+seqid*8)
        self.seq=bytearray(self.rom[0xbc5f60+off:0xbc5f60+off+size])
        ctl=self.rom[0x966260:0x979aa0]
        off,size=struct.unpack_from('>II',ctl,4+self.bankid*8)
        self.bank=ctl[off+16:off+size]
        self.instrument_count=u32(ctl,off); self.drum_count=u32(ctl,off+4)
        tbl=self.rom[0x979aa0:0xbc5f60]
        off,size=struct.unpack_from('>II',tbl,4+self.bankid*8)
        if not size: off,size=struct.unpack_from('>II',tbl,4+off*8)
        self.table=tbl[off:off+size];self.samples={}
    def envelope(self,ptr,sequence=False):
        buf=self.seq if sequence else self.bank; out=[]
        for i in range(64):
            d,v=struct.unpack_from('>hh',buf,ptr+4*i);out.append((d,v))
            if d in (-1,0,-3):break
        return out
    def sample(self,p):
        if p not in self.samples:
            address,loop,book,size=struct.unpack_from('>IIII',self.bank,p+4)
            order,predictors=struct.unpack_from('>II',self.bank,book)
            if order!=2 or not 1<=predictors<=16:raise ValueError('ADPCM book')
            coeff=struct.unpack_from('>'+str(16*predictors)+'h',self.bank,book+8)
            start,end,count=struct.unpack_from('>III',self.bank,loop)
            raw=self.table[address:address+size]
            if len(raw)!=size:raise ValueError('ADPCM source bounds')
            pcm=decode_adpcm(raw,coeff,predictors)
            if not 0<=start<=end<=len(pcm)+16:raise ValueError('ADPCM loop bounds')
            if len(pcm)<end:pcm=np.pad(pcm,(0,end-len(pcm)))
            self.samples[p]=(pcm[:end],start,end,count,dict(bank_pointer=p,table_offset=address,
              compressed_bytes=size,compressed_sha256=hashlib.sha256(raw).hexdigest(),loop_start=start,loop_end=end,loop_count=count))
        return self.samples[p]
    def sound(self,inst,note):
        b=self.bank
        if inst==127:
            if not 0<=note<self.drum_count:raise ValueError(f'drum {note}/{self.drum_count}')
            p=u32(b,u32(b,0)+4*note)
            if not p:raise ValueError('empty drum')
            return self.sample(u32(b,p+4)),f32(b,p+8),self.envelope(u32(b,p+12)),b[p],b[p+1],True
        if not 0<=inst<self.instrument_count:raise ValueError(f'instrument {inst}/{self.instrument_count}')
        p=u32(b,4+inst*4)
        if not p:raise ValueError('empty instrument')
        reg=8 if note<b[p+1] else (24 if note>b[p+2] else 16)
        return self.sample(u32(b,p+reg)),f32(b,p+reg+4),self.envelope(u32(b,p+4)),b[p+3],None,False

class Script:
    def __init__(self,seq,pc,kind,channel=None):
        self.seq=seq;self.pc=pc;self.kind=kind;self.channel=channel
        self.stack=[];self.delay=0;self.enabled=True;self.value=0;self.io=[-1]*16;self.dyntable=0
        self.inst=None;self.transpose=0;self.velocity=127;self.gate=0;self.last=1;self.default=1
        self.pan=64;self.env=None;self.release=None;self.slide=None;self.continuous=False;self.ignore_drum_pan=False
        self.layers=[None]*4;self.large=False;self.volume=1.;self.volscale=1.;self.weight=128
        self.freq=1.;self.vib_extent=0;self.vib_rate=0;self.vib_delay=0;self.reverb=0;self.stop=False
        self.active_note=None
    def byte(self):
        if not 0<=self.pc<len(self.seq.data):raise ValueError(f'PC {self.pc:x}')
        x=self.seq.data[self.pc];self.pc+=1;return x
    def word(self):return self.byte()*256+self.byte()
    def var(self):
        x=self.byte();return ((x&127)*256+self.byte())if x&128 else x
    def disable(self,tick):
        self.enabled=False
        if self.active_note is not None:self.active_note['end']=min(self.active_note['end'],tick)
        for l in self.layers:
            if l:l.disable(tick)
    def control(self,c,tick):
        if c==0xff:
            if self.stack:self.pc=self.stack.pop()[0]
            else:self.disable(tick)
        elif c==0xfc:
            p=self.word();self.stack.append((self.pc,None));self.pc=p
        elif c==0xf8:
            n=self.byte() or 256;self.stack.append((self.pc,n))
        elif c==0xf7:
            p,n=self.stack.pop();n-=1
            if n:self.stack.append((p,n));self.pc=p
        elif c==0xf6:self.stack.pop()
        elif c in (0xfb,0xfa,0xf9,0xf5):
            p=self.word();take=c==0xfb or (c==0xfa and self.value==0)or(c==0xf9 and self.value<0)or(c==0xf5 and self.value>=0)
            if take:
                if self.kind=='sequence' and not self.stack and c==0xfb and p<self.pc:
                    self.seq.loops.append((self.seq.pc_ticks.get(p,0),tick,p))
                    self.seq.loop_states.append(self.seq.control_state(tick))
                self.pc=p
        elif c in (0xf4,0xf3,0xf2):
            rel=signed(self.byte());take=c==0xf4 or(c==0xf3 and self.value==0)or(c==0xf2 and self.value<0)
            if take:self.pc+=rel
        else:return False
        if len(self.stack)>16:raise ValueError('script stack overflow')
        return True
    def run(self,tick):
        if not self.enabled or self.stop:return
        if self.delay>1:self.delay-=1;return
        self.delay=0
        for _ in range(4096):
            at=self.pc;c=self.byte();self.seq.ops[self.kind].add(c)
            if self.kind=='sequence':self.seq.pc_ticks.setdefault(at,tick)
            if c>=0xf0 and self.control(c,tick):
                if not self.enabled:return
                continue
            if self.kind=='layer':
                if c<=0xc0:
                    if self.active_note is not None and not self.continuous:self.active_note['end']=min(self.active_note['end'],tick)
                    if c==0xc0:self.delay=max(1,self.var());return
                    ch=self.channel;ty=c&0xc0;note=c&63
                    if ch.large:
                        if ty!=0x80:self.last=self.var()
                        vel=min(127,self.byte());self.gate=self.byte()if ty!=0x40 else 0
                    else:
                        if ty==0:self.last=self.var()
                        vel=self.velocity
                    delay=self.default if not ch.large and ty==0x40 else self.last
                    delay=max(1,delay);self.delay=delay
                    inst=ch.inst if self.inst is None else self.inst
                    if inst is None:raise ValueError(f'note without instrument at{at:x}, tick{tick}, channel{self.seq.channels.index(ch)}')
                    note+=ch.transpose+self.transpose+(0 if inst==127 else self.seq.transpose)
                    if not 0<=note<128:return
                    n=dict(tick=tick,end=tick+delay-((self.gate*delay)>>8),channel=self.seq.channels.index(ch),
                      layer_pc=at,inst=inst,note=note,velocity=vel,pan=self.pan,ignore_drum_pan=self.ignore_drum_pan,
                      env=self.env or ch.env,release=self.release or ch.release,slide=self.slide,continuous=self.continuous)
                    self.seq.notes.append(n);self.active_note=n;return
                if c==0xc1:self.velocity=self.byte()
                elif c==0xc2:self.transpose=signed(self.byte())
                elif c==0xc3:self.default=self.var()
                elif c in(0xc4,0xc5):self.continuous=c==0xc4
                elif c==0xc6:self.inst=self.byte()
                elif c==0xc7:
                    mode=self.byte();target=self.byte()+self.transpose+self.channel.transpose+self.seq.transpose
                    self.slide=(mode,target,self.byte()if mode&128 else self.var())
                elif c==0xc8:self.slide=None
                elif c==0xc9:self.gate=self.byte()
                elif c==0xca:self.pan=self.byte()
                elif c==0xcb:self.env=self.seq.donor.envelope(self.word(),True);self.release=self.byte()
                elif c==0xcc:self.ignore_drum_pan=True
                elif 0xd0<=c<=0xdf:self.velocity=self.seq.velocity[c&15]
                elif 0xe0<=c<=0xef:self.gate=self.seq.duration[c&15]
                else:raise ValueError(f'layer {c:02x}@{at:x}')
                continue
            if c in(0xfd,0xfe):self.delay=max(1,self.var()if c==0xfd else 1);return
            if c==0xcc:self.value=signed(self.byte())
            elif c==0xc9:self.value&=self.byte()
            elif c==0xc8:self.value=signed((self.value-self.byte())&255)
            elif c==0xf1:self.byte()
            elif c==0xf0:pass
            elif self.kind=='sequence':
                if c==0xdf:self.seq.transpose=signed(self.byte())
                elif c==0xde:self.seq.transpose+=signed(self.byte())
                elif c==0xdd:self.seq.tempo=max(1,self.byte())
                elif c==0xdc:self.seq.tempo=max(1,self.seq.tempo+signed(self.byte()))
                elif c==0xdb:self.seq.volume=self.byte()/127.
                elif c==0xd9:self.seq.volscale=signed(self.byte())/127.
                elif c==0xda:self.byte();self.word()
                elif c==0xd7:self.word()
                elif c==0xd6:
                    mask=self.word()
                    for i,ch in enumerate(self.seq.channels):
                        if mask&(1<<i)and ch:ch.disable(tick)
                elif c in(0xd5,0xd3,0xd0):self.byte()
                elif c in(0xd2,0xd1):
                    p=self.word();table=list(self.seq.data[p:p+16])
                    if c==0xd2:self.seq.velocity=table
                    else:self.seq.duration=table
                elif 0x90<=c<=0x9f:self.seq.start_channel(c&15,self.word(),tick)
                elif c<0x10:self.value=int(not self.seq.channels[c]or not self.seq.channels[c].enabled)
                elif 0x70<=c<=0x7f:self.seq.variation=self.value
                elif 0x80<=c<=0x8f:self.value=self.seq.variation
                elif 0x50<=c<=0x5f:self.value-=self.seq.variation
                else:raise ValueError(f'sequence {c:02x}@{at:x}')
            else:
                if c==0xc1:self.inst=self.byte()
                elif c in(0xc3,0xc4):self.large=c==0xc4
                elif c==0xc2:self.dyntable=self.word()
                elif c==0xc5:
                    if self.value!=-1:self.dyntable=u16(self.seq.data,self.dyntable+2*self.value)
                elif c in(0xc6,0xeb):
                    bank=self.byte()
                    if bank:raise ValueError('unexpected bank selector')
                    if c==0xeb:self.inst=self.byte()
                elif c==0xdf:self.volume=self.byte()/127.
                elif c==0xe0:self.volscale=self.byte()/128.
                elif c==0xde:self.freq=self.word()/32768.
                elif c==0xd3:self.freq=2**(signed(self.byte())/127.)
                elif c==0xdd:self.pan=self.byte()
                elif c==0xdc:self.weight=self.byte()
                elif c==0xdb:self.transpose=signed(self.byte())
                elif c==0xda:self.env=self.seq.donor.envelope(self.word(),True)
                elif c==0xd9:self.release=self.byte()
                elif c==0xd8:self.vib_extent=self.byte()*8
                elif c==0xd7:self.vib_rate=self.byte()*32
                elif c==0xe3:self.vib_delay=self.byte()*16
                elif c in(0xe1,0xe2):
                    start=self.byte();target=self.byte();delay=self.byte()*16
                    if c==0xe1:self.vib_rate=target*32
                    else:self.vib_extent=target*8
                    self.seq.approximations.add('vibrato target ramp')
                elif c==0xd4:self.reverb=self.byte()
                elif c==0xec:self.vib_extent=self.vib_rate=0;self.freq=1.
                elif c==0xc7:
                    add=self.byte();p=self.word();self.seq.data[p]=(self.value+add)&255
                elif c==0xcb:self.value=signed(self.seq.data[self.word()+self.value])
                elif c==0xe4:
                    if self.value!=-1:
                        p=u16(self.seq.data,self.dyntable+2*self.value);self.stack.append((self.pc,None));self.pc=p
                elif c in(0xca,0xd0,0xd1,0xd2,0xe5,0xe6,0xe9):self.byte()
                elif c in(0xe7,0xe8):
                    if c==0xe7:p=self.word();vals=self.seq.data[p:p+8]
                    else:vals=[self.byte()for i in range(8)]
                    self.transpose=signed(vals[3]);self.pan=vals[4];self.weight=vals[5];self.reverb=vals[6]
                elif c==0xef:self.word();self.byte()
                elif c==0xea:self.stop=True;return
                elif 0x90<=c<=0x93:
                    p=self.word();i=c&3
                    if self.layers[i]:self.layers[i].disable(tick)
                    self.layers[i]=Script(self.seq,p,'layer',self)
                elif 0xa0<=c<=0xa3:
                    if self.layers[c&3]:self.layers[c&3].disable(tick)
                    self.layers[c&3]=None
                elif 0xb0<=c<=0xb3:
                    if self.value!=-1:
                        i=c&3
                        if self.layers[i]:self.layers[i].disable(tick)
                        self.layers[i]=Script(self.seq,u16(self.seq.data,self.dyntable+2*self.value),'layer',self)
                elif 0x60<=c<=0x6f:self.delay=max(1,c&15);return
                elif 0x70<=c<=0x7f:self.io[c&15]=self.value
                elif 0x80<=c<=0x8f:
                    self.value=self.io[c&15]
                    if c&15<4:self.io[c&15]=-1
                elif 0x50<=c<=0x5f:self.value-=self.io[c&15]
                elif 0x00<=c<=0x03:self.value=int(not self.layers[c]or not self.layers[c].enabled)
                elif 0x10<=c<=0x1f:self.seq.start_channel(c&15,self.word(),tick)
                elif 0x20<=c<=0x2f:
                    if self.seq.channels[c&15]:self.seq.channels[c&15].disable(tick)
                elif 0x30<=c<=0x3f:self.seq.channels[c&15].io[self.byte()]=self.value
                elif 0x40<=c<=0x4f:self.value=self.seq.channels[c&15].io[self.byte()]
                else:raise ValueError(f'channel {c:02x}@{at:x}')
        raise ValueError('command budget')

class Sequence:
    def __init__(self,donor):
        self.donor=donor;self.data=donor.seq;self.channels=[None]*16;self.notes=[]
        self.tempo=120;self.transpose=0;self.volume=1.;self.volscale=1.;self.variation=0
        self.velocity=[12,25,38,51,57,64,71,76,83,89,96,102,109,115,121,127]
        self.duration=[229,203,177,151,125,99,73,47,21,0,0,0,0,0,0,0]
        self.main=Script(self,0,'sequence');self.loops=[];self.loop_states=[];self.pc_ticks={};self.times=[];self.curves=[[]for _ in range(16)]
        self.ops={k:set()for k in ('sequence','channel','layer')};self.approximations=set()
    def control_state(self,tick):
        def state(s):
            if s is None:return None
            result={k:copy.deepcopy(v)for k,v in vars(s).items()if k not in('seq','channel','layers','active_note')}
            result['layers']=[state(l)for l in s.layers]
            n=s.active_note
            result['active_note']=None if n is None else(n['tick']-tick,n['end']-tick,n['inst'],n['note'],n['velocity'],n['pan'])
            return result
        return dict(channels=[state(ch)for ch in self.channels],tempo=self.tempo,
          transpose=self.transpose,volume=self.volume,volscale=self.volscale,variation=self.variation,
          velocity=self.velocity.copy(),duration=self.duration.copy())
    def start_channel(self,i,p,tick):
        if self.channels[i]:
            ch=self.channels[i];ch.disable(tick);ch.pc=p;ch.stack=[];ch.delay=0;ch.stop=False;ch.enabled=True;ch.layers=[None]*4
        else:self.channels[i]=Script(self,p,'channel')
    def parse(self,loop_count=2):
        seconds=0.
        for tick in range(100000):
            self.times.append(seconds);self.main.run(tick)
            if len(self.loops)>=loop_count:break
            for i,ch in enumerate(self.channels):
                if ch and ch.enabled:
                    ch.run(tick)
                    for layer in ch.layers:
                        if layer:layer.run(tick)
                self.curves[i].append((ch.volume*ch.volscale*self.volume*self.volscale,ch.freq,ch.pan,ch.weight,ch.vib_extent,ch.vib_rate,ch.reverb)if ch else(0,1,64,128,0,0,0))
            seconds+=60/(self.tempo*48)
            if seconds>480:raise ValueError('song exceeds eight minutes without two loops')
        else:raise ValueError('no sequence loop')
        if len(self.loops)<2:raise ValueError('no repeatable sequence loop')
        if self.loop_states[-2]!=self.loop_states[-1]:raise ValueError(f'non-repeating channel/layer state for song{self.donor.seqid}')
        for ch in self.channels:
            if ch:ch.disable(tick)
        return self

def join_intro_pcm(pcm,intro):
    """Remove the one-time splice step using the same bounded smooth join."""
    blend=min(round(RATE*.005),intro)
    if blend:
        w=np.linspace(0,1,blend)[:,None];w=w*w*(3-2*w)
        delta=pcm[intro].astype(np.int64)-pcm[intro-1].astype(np.int64)
        values=pcm[intro-blend:intro].astype(np.float64)+w*delta
        pcm[intro-blend:intro]=np.clip(np.rint(values),-32767,32767).astype('<i2')
    return pcm

def render(seq):
    d=seq.donor;times=np.asarray(seq.times);frames=round(times[-1]*RATE)
    out=np.zeros((frames+RATE,2),np.float64);sources={};approx=seq.approximations
    channel_curves=[np.asarray(c) for c in seq.curves]
    for n in seq.notes:
        if n['end']<=n['tick']:continue
        sample,tuning,env,release,drum_pan,is_drum=d.sound(n['inst'],n['note'])
        pcm,ls,le,lc,meta=sample;sources[meta['bank_pointer']]=meta
        begin=round(times[n['tick']]*RATE);endtime=np.interp(n['end'],np.arange(len(times)),times)
        # ADSR release rate is measured in synthesis updates (three per VI).
        release=n['release']if n['release']is not None else release
        release=release or 32
        # heap.c: releaseRate * (0.001171875 / updatesPerFrame) per update.
        # At three updates per60Hz VI this is releaseRate *0.0703125/second.
        release_seconds=1/(release*.0703125)
        count=min(len(out)-begin,max(1,round((endtime-times[n['tick']]+release_seconds)*RATE)))
        tt=times[n['tick']]+np.arange(count)/RATE
        curve=channel_curves[n['channel']];axis=times[:len(curve)]
        freq=np.interp(tt,axis,curve[:,1]);ratios=tuning*(1 if is_drum else 2**((n['note']-39)/12))*32000/RATE*freq
        extent=np.interp(tt,axis,curve[:,4]);vrate=np.interp(tt,axis,curve[:,5])
        if np.any(extent):
            phase=np.cumsum(vrate)*180/(65536*RATE)*2*np.pi
            pitch_change=np.floor(127*np.sin(phase))
            ratios*=1+extent/4096*(2**((pitch_change+1)/127)-1)
        if n['slide']:
            mode,target,t=n['slide'];other=2**((target-n['note'])/12)
            start,stop=(other,1)if mode&127 in(1,3,5)else(1,other)
            span=max(1,count*t/127 if mode&128 else t*RATE/180)
            ratios*=start+(stop-start)*np.minimum(np.arange(count)/span,1)
        ratios=np.minimum(ratios,3.9999198)
        indices=np.cumsum(ratios)-ratios[0]
        if lc:indices=np.where(indices>=le,ls+(indices-le)%max(1,le-ls),indices)
        signal=np.interp(indices,np.arange(len(pcm)),pcm,right=0)
        envelope=n['env']or env;ramp=np.zeros(count);cursor=0;old=0.;ep=0
        for _ in range(128):
            if ep>=len(envelope):break
            delay,target=envelope[ep];ep+=1
            if delay==-1:ramp[cursor:]=old;break
            if delay==0:break
            if delay==-2:ep=target;continue
            if delay==-3:ep=0;continue
            updates=max(1,delay*3//4)if delay>=4 else delay
            length=max(1,round(updates/180*RATE));stop=min(count,cursor+length);level=(target/32767)**2
            ramp[cursor:stop]=np.linspace(old,level,length,endpoint=False)[:stop-cursor];cursor=stop;old=level
            if cursor>=count:break
        release_start=min(count,max(0,round((endtime-times[n['tick']])*RATE)))
        if release_start<count:
            level=ramp[max(0,release_start-1)]
            ramp[release_start:]=np.maximum(0,level-np.arange(count-release_start)*release*.0703125/RATE)
        volume=np.interp(tt,axis,curve[:,0]);pan=np.interp(tt,axis,curve[:,2]);weight=np.interp(tt,axis,curve[:,3])/128
        layer_pan=drum_pan if is_drum and not n['ignore_drum_pan']else n['pan']
        pan=np.clip((pan*weight+layer_pan*(1-weight))/127,0,1)
        signal*=ramp*(n['velocity']/127)**2*volume**2
        out[begin:begin+count,0]+=signal*np.cos(pan*np.pi/2)
        out[begin:begin+count,1]+=signal*np.sin(pan*np.pi/2)
    # Use the second complete loop so notes crossing the loop boundary retain
    # their tails. Preserve the original introduction before the loop starts.
    a,b,_=seq.loops[0];_,c,_=seq.loops[1]
    intro=round(times[a]*RATE);loop_start=round(times[b]*RATE);loop_end=round(times[c]*RATE)
    if abs((times[c]-times[b])-(times[b]-times[a]))>.05:raise ValueError('unstable musical loop')
    rendered=np.concatenate((out[:intro],out[loop_start:loop_end]))
    # Join the last sample to the actual loop-start sample, not to an advanced
    # sample inside the opening window. A smooth5ms correction preserves the
    # loop's duration and removes the boundary step without replaying a sliver.
    blend=min(round(RATE*.005),(len(rendered)-intro)//8)
    if blend:
        w=np.linspace(0,1,blend)[:,None]
        w=w*w*(3-2*w)
        rendered[-blend:]+=w*(rendered[intro]-rendered[-1])
    peak=float(np.abs(rendered).max());gain=min(.55,26000/max(1,peak));rendered*=gain
    pcm=join_intro_pcm(np.rint(rendered).astype('<i2'),intro)
    return pcm,intro,dict(sequence_id=d.seqid,bank_id=d.bankid,notes=len(seq.notes),
      source_sequence_sha256=hashlib.sha256(bytes(d.seq)).hexdigest(),loop_ticks=[a,b,c],loop_seconds=(times[c]-times[b]),
      intro_seconds=times[a],synthesis_gain=gain,peak=int(np.abs(pcm.astype(np.int32)).max()),
      command_ids={k:sorted(v)for k,v in seq.ops.items()},samples=list(sources.values()),approximations=sorted(approx))

def main():
    p=argparse.ArgumentParser();p.add_argument('rom',type=Path);p.add_argument('output',type=Path);p.add_argument('--parse-only',action='store_true')
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True);rom=args.rom.read_bytes();songs=[]
    bank=bytearray(b'R64MUS1\0'+struct.pack('<II',len(SEQUENCE_BANKS),len(COURSES)))
    for course,song in COURSES.items():bank+=course.encode().ljust(32,b'\0')+struct.pack('<I',song)
    for sid in SEQUENCE_BANKS:
        donor=MusicDonor(rom,sid);seq=Sequence(donor).parse()
        if args.parse_only:
            print(json.dumps(dict(sequence=sid,notes=len(seq.notes),seconds=seq.times[-1],loops=seq.loops,commands={k:sorted(v)for k,v in seq.ops.items()})),flush=True);continue
        pcm,loop,proof=render(seq)
        if proof['peak']<100:raise ValueError('silent song')
        with wave.open(str(args.output/f'sequence-{sid:02x}.wav'),'wb')as f:
            f.setnchannels(2);f.setsampwidth(2);f.setframerate(RATE);f.writeframes(pcm.tobytes())
        bank+=struct.pack('<IIII',sid,RATE,len(pcm),loop)+pcm.tobytes()
        proof.update(frames=len(pcm),loop_start_frame=loop,pcm_sha256=hashlib.sha256(pcm.tobytes()).hexdigest());songs.append(proof)
        print(json.dumps({k:proof[k]for k in ('sequence_id','notes','frames','loop_seconds','peak')}),flush=True)
    if args.parse_only:return
    (args.output/'course-music.bin').write_bytes(bank)
    report=dict(rom_sha256=hashlib.sha256(donor.rom).hexdigest(),bank_sha256=hashlib.sha256(bank).hexdigest(),bank_bytes=len(bank),sample_rate=RATE,
      course_songs=COURSES,songs=songs,method='Actual donor sequence/channel/layer notes and original VADPCM instruments, stereo host synthesis; no RSP reverb, host linear interpolation and continuous-time ADSR approximation.')
    (args.output/'extraction.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'bank_bytes':len(bank),'bank_sha256':report['bank_sha256']}))
if __name__=='__main__':main()
