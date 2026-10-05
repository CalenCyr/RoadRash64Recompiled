"""Extract native projection and the exact AI caller/retry boundary; no ROM data."""
from pathlib import Path
import argparse
import json
import re

p = argparse.ArgumentParser()
p.add_argument('generated', type=Path)
p.add_argument('output', type=Path)
p.add_argument('config', type=Path)
a = p.parse_args()
functions = {}
for path in a.generated.glob('funcs_*.c'):
    for m in re.finditer(r'RECOMP_FUNC void (\w+)\(.*?(?=RECOMP_FUNC void|\Z)',
                         path.read_text(), re.S):
        functions[m[1]] = m[0]

caller = functions['func_8004EB6C']
hook = 'rr64_course_ai_retry_projection(rdram,ctx);'
rows = [line for line in a.config.read_text().splitlines()
        if hook in line and not line.lstrip().startswith('#')]
assert len(rows) == 1 and 'func_8004EB6C' in rows[0] and '0x8004ED04' in rows[0]
marker = '    // 0x8004ED04:'
statement = json.loads(re.search(r'text\s*=\s*(".*")\s*\}', rows[0])[1])
assert caller.split(marker)[0].rstrip().endswith(statement.strip()), \
    'Regenerate native code before testing its retry hook'
start, end = caller.index('    // 0x8004ECDC:'), caller.index(marker)
part = caller[start:end]
assert part.count(hook) == 1, 'Projection retry must follow the native call once'

needed, pending = set(), ['func_8004F658', 'func_8005A9DC']
while pending:
    name = pending.pop()
    if name in needed:
        continue
    needed.add(name)
    pending.extend(re.findall(r'\b((?:func_\w+|_nsqrtf))\(rdram, ctx\)', functions[name]))

parts = [functions[name] for name in sorted(needed)]
for name, body in [('test_course_ai_projection', part),
                   ('test_course_ai_projection_before', part.replace(hook, ''))]:
    parts.append(f'RECOMP_FUNC void {name}(uint8_t* rdram,recomp_context* ctx) {{\n'
                 'uint64_t hi=0,lo=0,result=0; int c1cs=0;\n' + body + '\nreturn;\n}\n')
decls = '\n'.join(f'void {name}(uint8_t*,recomp_context*);' for name in sorted(needed))
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text('#include "recomp.h"\n#include "rr64_course_ai.hpp"\n'
                    'extern "C" {\n' + decls + '\n' + ''.join(parts) + '\n}\n')
print(json.dumps({'native_functions': len(needed), 'projection_retry_hook': 1}))
