"""Extract real Pak serialization, profile copies and native checksum code."""
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
names = {'func_8001F960', 'func_8001F990', 'func_8001F9BC', 'func_800207DC', 'func_80020BE8', 'func_80020ECC', 'func_80048484',
         'func_8005F420', 'func_8005F480', '_bcopy', '_bzero'}
functions = {}
for path in a.generated.glob('funcs_*.c'):
    for match in re.finditer(r'RECOMP_FUNC void (\w+)\(.*?(?=RECOMP_FUNC void|\Z)',
                             path.read_text(), re.S):
        if match[1] in names:
            functions[match[1]] = match[0]
assert functions.keys() == names

hooks = 0
for line in a.config.read_text().splitlines():
    if not re.search(r'rr64_campaign_(?:bonus_(?:new_profile|scan|capture|load|save)|menu_unlocks|passive_scan|restore_unlocks)\(', line) or line.lstrip().startswith('#'):
        continue
    match = re.fullmatch(r'\s*\{\s*func\s*=\s*"(\w+)"\s*,\s*'
                         r'(?:before_vram\s*=\s*(0x[0-9A-Fa-f]+)\s*,\s*)?'
                         r'text\s*=\s*(".*")\s*\},?\s*', line)
    assert match, ('Audit changed hook format', line)
    name = match[1]
    address = int(match[2], 16) if match[2] else int(name[5:], 16)
    statement = json.loads(match[3])
    marker = f'    // 0x{address:08X}:'
    code = functions[name]
    assert code.count(marker) == 1
    # A moved hook must replace its previous generated position. Otherwise
    # candidate mode silently retains the old entry hook as well as the fix.
    if a.candidate_config:
        code, count = re.subn(r'^\s*' + re.escape(statement) + r'\s*\n', '', code, flags=re.M)
        assert count <= 1, ('Duplicate generated persistence hook', name, statement)
    if not code.split(marker)[0].rstrip().endswith(statement):
        assert a.candidate_config, ('Missing generated persistence hook', name, address)
        code = code.replace(marker, '    ' + statement + '\n' + marker)
    functions[name] = code
    hooks += 1
assert hooks == 8

def slice_function(name, alias, start, end, tail=''):
    code = functions[name]
    first = code.index(f'    // 0x{start:08X}:')
    finish = code.index(f'    // 0x{end:08X}:', first)
    # Include injected hooks immediately before the first native instruction.
    prefix = code[:first]
    while prefix.splitlines()[-1].strip().startswith('rr64_campaign_bonus_'):
        first = prefix.rfind('\n', 0, len(prefix.rstrip('\n'))) + 1
        prefix = code[:first]
    return (f'RECOMP_FUNC void {alias}(uint8_t* rdram, recomp_context* ctx) {{\n'
            'uint64_t hi=0,lo=0,result=0; int c1cs=0;\n' + code[first:finish] +
            '\n' + tail + '\n}\n')

parts = [functions[n] for n in ('func_8001F960', 'func_8001F990', 'func_8001F9BC',
                               '_bcopy', '_bzero', 'func_8005F420', 'func_8005F480',
                               'func_800207DC', 'func_80020ECC')]
parts.append(slice_function('func_80020BE8', 'test_bonus_native_save',
                           0x80020E04, 0x80020E60))
# Run the real mode commit and its hook up to, but not into, the menu initializer.
parts.append(slice_function('func_80048484', 'test_campaign_menu_entry',
                           0x800484E4, 0x80048504))
code = ''.join(parts)
calls = set(re.findall(r'^\s+(\w+)\(rdram, ctx\);', code, re.M))
decls = '\n'.join(f'void {name}(uint8_t*,recomp_context*);' for name in sorted(calls)
                  if not name.startswith('rr64_'))
output = ('#include "recomp.h"\n#include "rr64_native.hpp"\nextern "C" {\n' +
          decls + '\n' + code + '\n}\n')
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(output)
print(f'Extracted eight save/unlock hooks, native menu entry, full scan/cache/error paths, checksum, 248-byte copies and 256-byte Pak I/O; candidate={a.candidate_config}')
