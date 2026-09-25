"""Render selected MK64 US course effects from a user-provided ROM.

This is deliberately a bounded SFX renderer, not a music/sequencer replacement.
The pinned donor's sound IDs select actual sequence layers, ADPCM instruments,
pitch, timing and envelopes. Its RSP reverb/resampler are not emulated: exported
mono PCM uses linear resampling and a short release. No assets ship in source.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import struct
import wave
from pathlib import Path
import numpy as np

RATE = 26800  # gAudioSessionPresets: 0x68b0
TICKS = 96   # sequence00: tempo120, 48 tatums/beat
EFFECTS = [
    ("item_break", 1, 0x06, False, 1.6),
    ("roulette", 0, 0x1c, True, 2.5),
    ("item_chosen", 0, 0x47, False, 2.0),
    ("train_whistle", 1, 0x0d, False, 4.0),
    ("train_whistle_double", 1, 0x0e, False, 4.0),
    ("crossing_bell", 1, 0x16, False, 2.0),
    ("ferry_whistle", 1, 0x47, False, 4.0),
    ("ferry_whistle_double", 1, 0x48, False, 4.0),
    ("wood_roll", 0, 0x23, True, 2.0),
    ("thwomp_impact", 1, 0x0f, False, 2.0),
    ("penguin", 1, 0x17, False, 2.0),
    ("mole", 1, 0x07, False, 1.5),
    ("boardwalk_roll", 0, 0x1e, True, 2.0),
]

def u16(b, p): return struct.unpack_from(">H", b, p)[0]
def u32(b, p): return struct.unpack_from(">I", b, p)[0]
def f32(b, p): return struct.unpack_from(">f", b, p)[0]

def decode_adpcm(data, coefficients, predictors):
    """Nintendo 9-byte/16-sample VADPCM, order two; arithmetic saturates s16."""
    previous = [0] * 16
    output = []
    for frame in range(len(data) // 9):
        block = data[frame * 9:frame * 9 + 9]
        scale, predictor = block[0] >> 4, block[0] & 15
        if predictor >= predictors or scale > 12:
            raise ValueError("Unsupported/corrupt VADPCM frame")
        code = coefficients[predictor * 16:predictor * 16 + 16]
        residuals = []
        for byte in block[1:]:
            for nibble in (byte >> 4, byte & 15):
                residuals.append((nibble if nibble < 8 else nibble - 16) << scale)
        decoded = []
        for half in range(2):
            history = previous[-2:] if not half else decoded[-2:]
            residual = residuals[half * 8:half * 8 + 8]
            for i in range(8):
                acc = residual[i] << 11
                acc += code[i] * history[0] + code[8 + i] * history[1]
                for j in range(i):
                    acc += code[8 + i - j - 1] * residual[j]
                decoded.append(max(-32768, min(32767, acc >> 11)))
        output.extend(decoded)
        previous = decoded
    return np.asarray(output, dtype=np.float64)

class Donor:
    def __init__(self, rom):
        if rom[:4] == bytes.fromhex("37804012"):
            rom = bytes(v for pair in zip(rom[1::2], rom[::2]) for v in pair)
        elif rom[:4] == bytes.fromhex("40123780"):
            rom = b"".join(rom[i:i+4][::-1] for i in range(0, len(rom), 4))
        if rom[:4] != bytes.fromhex("80371240") or rom[0x3b:0x3f] != b"NKT E".replace(b" ", b""):
            raise ValueError("Expected Mario Kart64 US big-endian cartridge image")
        self.rom = rom
        if hashlib.sha256(rom).hexdigest() != 'd6b8538dd63f0132ecb2856e7d32816ed3c30e3e479aecd23cf83fb6ba17a5da':
            raise ValueError('Unsupported Mario Kart64 ROM revision')
        self.seq = rom[0xbc6060:0xbc8890]
        ctl = rom[0x966260:0x979aa0]
        offset, length = struct.unpack_from(">II", ctl, 4)
        self.bank = ctl[offset + 16:offset + length]
        self.table = rom[0x979aa0 + 0xb0:0xbc5f60]
        self.samples = {}
        self.notes = []

    def instrument(self, inst, note):
        if not 0 <= inst < 127: raise ValueError(f"Unsupported instrument {inst}")
        b = self.bank
        ptr = u32(b, 4 + inst * 4)
        if not ptr: raise ValueError("Empty instrument")
        region = 8 if note < b[ptr+1] else (24 if note > b[ptr+2] else 16)
        sample, tuning = u32(b, ptr+region), f32(b, ptr+region+4)
        if sample not in self.samples:
            address, loop, book, size = struct.unpack_from(">IIII", b, sample+4)
            order, predictors = struct.unpack_from(">II", b, book)
            if order != 2 or not 1 <= predictors <= 16: raise ValueError("VADPCM book")
            coeff = struct.unpack_from(">"+str(16*predictors)+"h", b, book+8)
            start, end, count = struct.unpack_from(">III", b, loop)
            raw = self.table[address:address+size]
            pcm = decode_adpcm(raw, coeff, predictors)
            # Non-looping banks may name the first padded sample after the
            # final compressed frame (for example item-break ends at30129).
            if not 0 <= start <= end <= len(pcm)+16: raise ValueError("Sample loop")
            if end > len(pcm): pcm=np.pad(pcm,(0,end-len(pcm)))
            self.samples[sample] = (pcm[:end], start, end, count,
                {"bank_pointer":sample,"table_offset":address,"compressed_bytes":size,
                 "compressed_sha256":hashlib.sha256(raw).hexdigest(), "order":order,
                 "predictors":predictors, "loop_start":start,"loop_end":end,"loop_count":count})
        envelope = []
        env = u32(b, ptr+4)
        for i in range(32):
            delay, value = struct.unpack_from(">hh", b, env+i*4)
            envelope.append((delay,value))
            if delay < 0: break
        return self.samples[sample], tuning, envelope

    def layer(self, offset, large, inst, volume, duration, start=0):
        s = self.seq; pc = offset; tick = start; stack = []; notes = []
        velocity = 127; last_delay = default_delay = 1; gate = transpose = 0
        env = None; slide = None
        def byte():
            nonlocal pc
            v = s[pc]; pc += 1; return v
        def word(): return (byte()<<8)|byte()
        def var():
            n=byte(); return ((n&127)<<8)|byte() if n&128 else n
        for _ in range(12000):
            if tick >= duration*TICKS: break
            at=pc; cmd=byte()
            if cmd == 0xff:
                if not stack: break
                pc=stack.pop()[0]
            elif cmd == 0xfc:
                target=word(); stack.append((pc,0)); pc=target
            elif cmd == 0xfb: pc=word()
            elif cmd == 0xf4:
                n=byte(); pc += n if n<128 else n-256
            elif cmd == 0xf8: stack.append((pc+1,byte() or 256))
            elif cmd == 0xf7:
                target,n=stack.pop(); n-=1
                if n: stack.append((target,n)); pc=target
            elif cmd == 0xc6: inst=byte()
            elif cmd == 0xc1: velocity=byte()
            elif cmd == 0xc2: transpose=byte()
            elif cmd == 0xc3: default_delay=var()
            elif cmd == 0xc9: gate=byte()
            elif cmd == 0xca: byte()
            elif cmd in (0xc4,0xc5,0xcc): pass
            elif cmd == 0xc7:
                mode=byte(); target=byte(); time=byte() if mode&128 else var()
                slide=(mode,target,time)
            elif cmd == 0xc8: slide=None
            elif cmd == 0xcb:
                p=word(); byte(); env=[]
                for i in range(32):
                    d,v=struct.unpack_from(">hh",s,p+i*4); env.append((d,v))
                    if d<0: break
            elif 0xd0<=cmd<=0xdf:
                velocity=[12,25,38,51,57,64,71,76,83,89,96,102,109,115,121,127][cmd&15]
            elif 0xe0<=cmd<=0xef:
                gate=[229,203,177,151,139,126,113,100,87,74,61,48,36,23,10,0][cmd&15]
            elif cmd == 0xc0: tick+=var()
            elif cmd < 0xc0:
                kind=cmd&0xc0; note=(cmd&63)+transpose
                if large:
                    delay=var() if kind!=0x80 else last_delay
                    velocity=byte(); gate=byte() if kind!=0x40 else 0
                else: delay=var() if kind==0 else (default_delay if kind==0x40 else last_delay)
                last_delay=delay
                if not delay: raise ValueError("Zero note duration")
                notes.append(dict(pc=at,tick=tick,delay=delay,duration=delay*(1-gate/256),
                                  note=note,inst=inst,velocity=min(127,velocity),
                                  volume=volume,envelope=env,slide=slide))
                tick+=delay
            else: raise ValueError(f"Unknown layer command {cmd:x}@{at:x}")
        else: raise ValueError("Layer budget")
        return notes

    def effect(self, group, id, seconds):
        table={0:0x12f,1:0x6b3}[group]
        pc=u16(self.seq,table+id*2); entry=pc
        inst=0; volume=127; large=group==1; notes=[]; tick=0
        for _ in range(100):
            cmd=self.seq[pc]; pc+=1
            if cmd==0xff: break
            if cmd==0xc1: inst=self.seq[pc]; pc+=1
            elif cmd==0xdf: volume=self.seq[pc]; pc+=1
            elif cmd==0xc4: large=True
            elif cmd==0xc3: large=False
            elif 0x90<=cmd<=0x93:
                layer=u16(self.seq,pc); pc+=2
                notes+=self.layer(layer,large,inst,volume,seconds,tick)
            elif cmd in (0xdc,0xd8,0xd7,0xd2,0xd9,0xd4,0xcc): pc+=1
            elif cmd==0x76: pass
            elif 0x61<=cmd<=0x6f: tick+=cmd&15
            else: raise ValueError(f"Unsupported channel {cmd:x}@{pc-1:x}")
        if not notes: raise ValueError("No effect notes")
        signal=np.zeros(math.ceil(seconds*RATE),np.float64)
        used=[]; end_sample=0
        for event in notes:
            (pcm,loop_start,loop_end,loop_count,meta),tuning,envelope=self.instrument(event['inst'],event['note'])
            used.append(meta)
            begin=round(event['tick']/TICKS*RATE)
            count=min(len(signal)-begin,round(event['duration']/TICKS*RATE))
            if count<=0:continue
            # synthesis.c multiplies notes by heap.c's32000/device-rate.
            ratio=tuning*2**((event['note']-39)/12)*32000/RATE
            if event['slide']:
                mode,target,time=event['slide']; other=tuning*2**((target-39)/12)*32000/RATE
                # mode1/3/5 begin at target, mode2/4 begin at the note.
                initial,final=(other,ratio) if mode&127 in (1,3,5) else (ratio,other)
                span=max(1,count*(time/127) if mode&128 else time/TICKS*RATE)
                ratios=initial+(final-initial)*np.minimum(np.arange(count)/span,1)
                indices=np.cumsum(ratios)-ratios[0]
            else: indices=np.arange(count)*ratio
            if loop_count:
                indices=np.where(indices>=loop_end,loop_start+(indices-loop_end)%max(1,loop_end-loop_start),indices)
            values=np.interp(indices,np.arange(len(pcm)),pcm,right=0)
            env=event['envelope'] or envelope
            ramp=np.ones(count); cursor=0; old=0.; held=False
            for delay,target in env:
                if delay==-1: held=True;break
                if delay==0: ramp[cursor:]=0;break
                if delay<0: raise ValueError("Unsupported ADSR control")
                updates=max(1,delay*3//4) if delay>=4 else delay
                length=max(1,round(updates/180*RATE)); stop=min(count,cursor+length)
                level=(target/32767)**2
                ramp[cursor:stop]=np.linspace(old,level,length,endpoint=False)[:stop-cursor]
                cursor=stop;old=level
                if cursor==count:break
                ramp[cursor:]=old
            # Match note velocity/volume; short fade prevents cutting a sustained loop.
            fade=min(count,round(.006*RATE)); ramp[-fade:]*=np.linspace(1,0,fade)
            signal[begin:begin+count]+=values*ramp*(event['velocity']/127)**2*(event['volume']/127)
            end_sample=max(end_sample,begin+count)
        pcm=np.clip(np.rint(signal[:end_sample]*.7),-32768,32767).astype('<i2')
        return pcm,dict(sequence_entry=entry,notes=notes,samples=list({m['bank_pointer']:m for m in used}.values()))

def main():
    p=argparse.ArgumentParser();p.add_argument('rom',type=Path);p.add_argument('output',type=Path)
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    donor=Donor(args.rom.read_bytes()); bank=bytearray(b'R64SFX1\0'+struct.pack('<I',len(EFFECTS)))
    report={'rom_sha256':hashlib.sha256(donor.rom).hexdigest(),'sample_rate':RATE,
            'method':'Donor sequence notes, tuning, envelopes and VADPCM sample bytes; linear resampling, no donor RSP reverb', 'effects':[]}
    for index,(name,group,id,loop,seconds) in enumerate(EFFECTS):
        pcm,proof=donor.effect(group,id,seconds)
        if not len(pcm) or np.max(np.abs(pcm.astype(np.int32)))<100: raise ValueError('Silent effect '+name)
        with wave.open(str(args.output/(name+'.wav')),'wb') as f:
            f.setnchannels(1);f.setsampwidth(2);f.setframerate(RATE);f.writeframes(pcm.tobytes())
        bank+=struct.pack('<IIII',index,RATE,len(pcm),int(loop))+pcm.tobytes()
        report['effects'].append(dict(id=index,name=name,sound_group=group,sound_id=id,loop=loop,frames=len(pcm),sha256=hashlib.sha256(pcm.tobytes()).hexdigest(),**proof))
    (args.output/'course-audio.bin').write_bytes(bank)
    report['bank_sha256']=hashlib.sha256(bank).hexdigest()
    (args.output/'extraction.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'effects':len(EFFECTS),'bank_bytes':len(bank),'bank_sha256':report['bank_sha256']}))

if __name__=='__main__':main()
