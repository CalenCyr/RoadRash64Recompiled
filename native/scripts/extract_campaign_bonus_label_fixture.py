"""Extract native slot metadata plus all three label/unlock instruction blocks."""
from pathlib import Path
import argparse
import json
import re

p = argparse.ArgumentParser()
p.add_argument('generated', type=Path)
p.add_argument('output', type=Path)
p.add_argument('config', type=Path)
p.add_argument('--candidate-config', action='store_true')
a = p.parse_args()
names = {'func_8001F960', 'func_8002073C', 'func_8005F420',
         'func_80026F8C', 'func_80029DB8'}
functions = {}
for path in a.generated.glob('funcs_*.c'):
    for match in re.finditer(r'RECOMP_FUNC void (\w+)\(.*?(?=RECOMP_FUNC void|\Z)',
                             path.read_text(), re.S):
        if match[1] in names:
            functions[match[1]] = match[0]
assert functions.keys() == names

hooks = {}
for line in a.config.read_text().splitlines():
    if 'rr64_campaign_bonus_record(' not in line or line.lstrip().startswith('#'):
        continue
    match = re.fullmatch(r'\s*\{\s*func\s*=\s*"(\w+)"\s*,\s*'
                         r'before_vram\s*=\s*(0x[0-9A-Fa-f]+)\s*,\s*'
                         r'text\s*=\s*(".*")\s*\},?\s*', line)
    assert match, ('Audit changed label hook format', line)
    name, address, statement = match[1], int(match[2], 16), json.loads(match[3])
    assert address not in hooks
    hooks[address] = statement
    marker = f'    // 0x{address:08X}:'
    code = functions[name]
    assert code.count(marker) == 1
    if not code.split(marker)[0].rstrip().endswith(statement):
        assert a.candidate_config, ('Missing generated label hook', name, address)
        code = code.replace(marker, '    ' + statement + '\n' + marker)
    functions[name] = code
assert set(hooks) == {0x800272EC, 0x8002AA0C, 0x8002B074}


def block(name, alias, start, end, hook_address):
    code = functions[name]
    first = code.index(f'    // 0x{start:08X}:')
    finish = code.index(f'    // 0x{end:08X}:', first)
    body = code[first:finish]
    statement = '    ' + hooks[hook_address] + '\n'
    assert body.count(statement) == 1
    parts = []
    for suffix, content in (('', body), ('_original', body.replace(statement, ''))):
        parts.append(f'RECOMP_FUNC void {alias}{suffix}(uint8_t* rdram, recomp_context* ctx) {{\n'
                     'uint64_t hi=0,lo=0,result=0; int c1cs=0;\n' + content + '\nreturn;\n}\n')
    return ''.join(parts)


parts = [functions[n] for n in ('func_8001F960', 'func_8002073C', 'func_8005F420')]
parts.append(block('func_80026F8C', 'test_bonus_load_label',
                   0x800272E4, 0x80027304, 0x800272EC))
parts.append(block('func_80029DB8', 'test_bonus_save_label',
                   0x8002AA04, 0x8002AA24, 0x8002AA0C))
parts.append(block('func_80029DB8', 'test_bonus_overwrite_label',
                   0x8002B06C, 0x8002B08C, 0x8002B074))
code = ''.join(parts)
calls = set(re.findall(r'^\s+(\w+)\(rdram, ctx\);', code, re.M))
decls = '\n'.join(f'void {name}(uint8_t*,recomp_context*);' for name in sorted(calls))
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text('#include "recomp.h"\n#include "rr64_native.hpp"\nextern "C" {\n' +
                    decls + '\n' + code + '\n}\n')
print(f'Extracted native slot metadata and three original/patched label-unlock blocks; candidate={a.candidate_config}')
