"""Extract native selector calls and graph traversal without injecting hooks."""
from pathlib import Path
import argparse
import hashlib
import json
import re

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('generated', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--without-rider-preview-hooks', action='store_true')
a = p.parse_args()
names = {'func_80024EB8', 'func_8000FF64', 'func_8000F9E8', 'func_8000F958',
         'func_8000F9B0', 'func_8000F7F4', 'func_8001A634',
         'func_8000F594', 'func_8000E65C', 'func_8000E91C', 'func_8000E780',
         'func_8000EC30', 'func_80016A18', 'func_80016B84', 'func_80016DE8',
         'func_80015A90', 'n_alSeqpDelete'}
functions = {}
for path in a.generated.glob('funcs_*.c'):
    for m in re.finditer(r'RECOMP_FUNC void (\w+)\(.*?(?=\nRECOMP_FUNC |\Z)', path.read_text(), re.S):
        if m[1] in names:
            assert m[1] not in functions
            functions[m[1]] = m[0]
assert functions.keys() == names, names - functions.keys()
original_hashes = {n:hashlib.sha256(v.encode()).hexdigest() for n,v in functions.items()}
caller = functions.pop('func_80024EB8')
start, end = (caller.index(f'    // 0x{pc:08X}:') for pc in (0x800252F0, 0x80025308))
caller = caller[start:end]
assert caller.count('func_8000F9E8(rdram, ctx);') == 1
assert caller.count('func_8000FF64(rdram, ctx);') == 1
removed = 0
if a.without_rider_preview_hooks:
    functions['func_8000FF64'], removed = re.subn(
        r'^\s*rr64_rider_skin_preview_(?:begin|end)\([^;]+;\s*$', '',
        functions['func_8000FF64'], flags=re.M)
    assert removed == 2, 'Negative control requires the two real generated rider hooks.'
parts = list(functions.values())
parts.append('void fixture_selection_preview_call(uint8_t*rdram,recomp_context*ctx){\n' + caller + '\nreturn;\n}\n')
code = '\n'.join(parts)
calls = set(re.findall(r'^\s+(\w+)\(rdram, ctx\);', code, re.M))
decls = '\n'.join(f'void {n}(uint8_t*,recomp_context*);' for n in sorted(calls))
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text('#include "recomp.h"\n#include "rr64_native.hpp"\n' + decls + '\n' + code,
                    encoding='utf-8', newline='\n')
a.output.with_suffix('.json').write_text(json.dumps(dict(
    generatedFunctionSHA256=original_hashes,
    callerRange=['0x800252F0', '0x80025308'],
    negativeHooksRemoved=removed,
    candidateHooksInjected=False,
    scope='Exact native showroom bike/rider calls; complete graph traversal, material resolution/cache/CI8 emission, quaternion transform and matrix/geometry emission. Only resource loading, libultra matrix packing/copy and unrelated host hooks are controlled by the test.'), indent=2) + '\n')
