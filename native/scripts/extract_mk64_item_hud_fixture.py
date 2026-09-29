"""Extract complete native sprite queue producer, consumer and rectangle renderer."""
from pathlib import Path
import argparse,hashlib,json,re

p=argparse.ArgumentParser()
p.add_argument('generated',type=Path)
p.add_argument('output',type=Path)
p.add_argument('config',type=Path)
p.add_argument('--candidate-config',action='store_true')
a=p.parse_args()
names={'8001EB90','8001EF8C','8001E898','8001CFB8'}
selected={}
weapon_caller=None
for path in a.generated.glob('funcs_*.c'):
    for m in re.finditer(r'RECOMP_FUNC void func_([A-F0-9]+)\(.*?(?=\nRECOMP_FUNC |\Z)',path.read_text(),re.S):
        if m[1] in names:selected[m[1]]=m[0]
        if m[1]=='80030220':weapon_caller=m[0]
assert selected.keys()==names
assert weapon_caller
hooks=[]
configuration=a.config.read_text()
assert 'rr64_mk64_items_hud(rdram' not in configuration,'Retired separate MK overlay is still invoked'
for line in configuration.splitlines():
    if 'rr64_mk64_item_hud_draw_record' not in line or line.lstrip().startswith('#'):continue
    h=re.fullmatch(r'\s*\{\s*func\s*=\s*"func_(\w+)"\s*,\s*before_vram\s*=\s*(0x[0-9A-Fa-f]+)\s*,\s*text\s*=\s*(".*")\s*\},?\s*',line)
    assert h and h[1]=='8001EF8C' and int(h[2],16)==0x8001F19C
    hook=json.loads(h[3]);marker=f'    // 0x{int(h[2],16):08X}:'
    pieces=selected[h[1]].split(marker)
    assert len(pieces)==2
    if not pieces[0].rstrip().endswith(hook.strip()):
        assert a.candidate_config,'Missing generated production HUD consumer hook'
        pieces[0]+=hook+'\n'
    selected[h[1]]=marker.join(pieces)
    hooks.append(dict(function=h[1],address=h[2],text=hook))
assert len(hooks)==1,'Expected exactly one configured queue consumer hook'
# Retain the game's actual weapon-call stack stores, delay slots and hooks.
# The old fixture swapped caller +40/+44 and missed an integration failure.
weapon_slices={}
for name,begin,end in [('single',0x80032288,0x800322E0),
                       ('split_a',0x80032E30,0x80032E94),
                       ('split_b',0x800330C0,0x80033124)]:
    chunk=weapon_caller.split(f'    // 0x{begin:08X}:',1)[1].split(f'    // 0x{end:08X}:',1)[0]
    chunk=f'    // 0x{begin:08X}:'+chunk
    assert 'rr64_course_items_hud_sprite(rdram' in chunk
    assert 'rr64_course_items_hud_sprite_end(rdram)' in chunk
    assert 'MEM_W(0X40, ctx->r29) = 0;' in chunk
    assert 'MEM_W(0X44, ctx->r29) = ctx->r2;' in chunk
    weapon_slices[name]=chunk
calls=set(re.findall(r'func_([A-F0-9]+)\(rdram, ctx\)',''.join(selected.values())))
external=calls-names
assert external=={'8001E948','8001677C','8001C084','8001CE80','8001CEFC'}
text='#include "recomp.h"\n#include "rr64_native.hpp"\n'
text+='void fixture_hud_native_stub(unsigned,unsigned char*,recomp_context*);\n'
text+=''.join(f'void func_{n}(unsigned char*,recomp_context*);\n' for n in sorted(calls|names))
text+=''.join(f'void func_{n}(unsigned char*m,recomp_context*c){{fixture_hud_native_stub(0x{n}u,m,c);}}\n' for n in sorted(external))
text+='\n'.join(selected.values())
for name,chunk in weapon_slices.items():
    text+=f'\nvoid fixture_hud_weapon_{name}(unsigned char*rdram,recomp_context*ctx){{\n'+chunk+'\n}\n'
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(text)
a.output.with_suffix('.json').write_text(json.dumps(dict(candidateHooks=a.candidate_config,
    functions={n:hashlib.sha256(s.encode()).hexdigest() for n,s in selected.items()},
    hooks=hooks,externalStubs=sorted(external),
    weaponCallSites={name:hashlib.sha256(chunk.encode()).hexdigest() for name,chunk in weapon_slices.items()},
    scope='Full native producer/consumer/descriptor lookup/rectangle emission; IO, viewport and unused async texture preparation stubs only'),indent=2)+'\n')
