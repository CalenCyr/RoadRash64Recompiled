"""Compile actual generated sprite emitter and its two-hook negative control.

Usage: python scripts/test_sprite_bounds.py --rom build/roadrash64.us.z64
       --cxx clang++ --output /path/to/private-test-dir [--negative]
No ROM pixels or generated artifacts are written into the source tree.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rom', type=Path, required=True)
parser.add_argument('--cxx', required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--negative', action='store_true')
args = parser.parse_args()
source = Path(__file__).resolve().parents[1]
args.output.mkdir(parents=True, exist_ok=True)
generated = (source / 'build/RecompiledFuncs/funcs_6.c').read_text()
code = re.search(r'RECOMP_FUNC void func_8001CFB8\(.*?(?=RECOMP_FUNC void)', generated, re.S)[0]
assert not re.search(r'\w+\(rdram, ctx\)', code), 'Update fixture if native emitter gains callees'
variables = re.findall(r'    gpr (jr_addend_\w+) = ', code)
for variable in variables:
    code = code.replace('    gpr ' + variable + ' = ', '    ' + variable + ' = ')
point = code.index('{') + 1
code = code[:point] + '\n' + ''.join('gpr ' + v + '=0;\n' for v in variables) + code[point:]
original = code.replace('func_8001CFB8(', 'original_8001CFB8(')
for pc, reg in [('8001E0D4', 2), ('8001E0E0', 3)]:
    hook = f'    ctx->r{reg} = ADD32(ctx->r{reg}, -4);\n    // 0x{pc}:'
    assert original.count(hook) == 1, 'Generated sprite hook missing: ' + pc
    original = original.replace(hook, f'    // 0x{pc}:')
native = args.output / 'sprite_native.cpp'
native.write_text('#include "recomp.h"\n#undef RECOMP_FUNC\n#define RECOMP_FUNC\nextern "C" {\n' + code + original + '}\n')
exe = args.output / ('sprite_bounds.exe' if sys.platform == 'win32' else 'sprite_bounds')
target = ['--target=x86_64-pc-windows-msvc'] if sys.platform == 'win32' else []
subprocess.run([args.cxx, *target, '-std=c++20', '-O1', '-DNDEBUG', '-I', str(source / 'native/lib/N64ModernRuntime/N64Recomp/include'),
                str(source / 'native/tests/rr64_sprite_bounds_smoke.cpp'), str(native), '-o', str(exe)], check=True)
sys.exit(subprocess.run([str(exe.resolve()), str(args.rom.resolve())] + (['--negative'] if args.negative else [])).returncode)
