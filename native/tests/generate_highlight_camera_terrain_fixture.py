"""Extract exact native floor closure and generate asset-free collision cases."""
from pathlib import Path
import argparse, hashlib, json, re, sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(a.source/'scripts'))
from mk64_importer.native_cell import Vertex, Triangle, encode_cell

functions = {}
for path in (a.source/'build/RecompiledFuncs').glob('funcs_*.c'):
    for m in re.finditer(r'^RECOMP_FUNC void (\w+)\([^\n]+\) \{.*?(?=^RECOMP_FUNC|\Z)', path.read_text(), re.M|re.S):
        functions[m[1]] = m[0]
pending = ['func_80014604', 'func_80014DE4']; selected = set()
while pending:
    name = pending.pop()
    if name in selected: continue
    selected.add(name)
    pending += re.findall(r'\b(func_\w+)\(rdram, ctx\)', functions[name])
assert len(selected) == 13
body = '\n'.join(functions[n] for n in sorted(selected))
assert 'LOOKUP_FUNC' not in body
assert set(re.findall(r'\brr64_\w+', body)) == {
    'rr64_online_terrain_query_begin', 'rr64_online_terrain_lookup',
    'rr64_highlight_camera_floor_cell', 'rr64_experimental_course_floor_indices',
    'rr64_experimental_course_floor_subindices', 'rr64_experimental_course_floor_cell_allowed'}
(a.output/'native.c').write_text('#include "recomp.h"\n#include "rr64_native.hpp"\n#include "funcs.h"\n'+body)
policy = (a.source/'native/src/rr64_experimental_course_policy.cpp').read_text()
region = policy[policy.index('struct FloorIndices'):policy.index('// Pack selection does not change')]
(a.output/'policy.cpp').write_text('#include "rr64_experimental_course.hpp"\n#include "rr64_engine_layout.hpp"\n#include <cmath>\nnamespace {\n'+region)

def square(z, surface=5, reverse=False):
    v = [Vertex(x,y,z(x)) for x,y in [(0,0),(1000,0),(1000,1000),(0,1000)]]
    groups = [(v[0],v[2],v[1]),(v[0],v[3],v[2])]
    return [Triangle(tuple(reversed(t)) if reverse else t,surface=surface) for t in groups]
floor, deck, ceiling = square(lambda x:200), square(lambda x:320), square(lambda x:320, reverse=True)
cases = dict(flat=floor, steep=square(lambda x:200+x//2), stacked=floor+deck,
             stacked_reverse=deck+floor, tunnel=floor+ceiling, ceiling=ceiling,
             empty=[Triangle(t.vertices,surface=None) for t in floor], surface=square(lambda x:200,surface=13))
header=[]
for name, triangles in cases.items():
    data=encode_cell(35*70+35,triangles)
    header.append(f'static const unsigned char cell_{name}[] = {{'+','.join(map(str,data))+'};')
(a.output/'rr64_highlight_camera_terrain_inputs.hpp').write_text('\n'.join(header)+'\n')
(a.output/'native-provenance.json').write_text(json.dumps(dict(
    functions={n:hashlib.sha256(functions[n].encode()).hexdigest() for n in sorted(selected)},
    substitutions='No native instruction changes. Fixture only stubs terrain-bank guards (tested separately) and sqrt runtime.',
    synthetic_cases=list(cases), embedded_rom_assets=False),indent=2)+'\n')
