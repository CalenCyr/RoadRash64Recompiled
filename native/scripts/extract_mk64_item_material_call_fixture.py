"""Extract the two native actor-DL calls and RT64's real address resolver."""
from pathlib import Path
import argparse
import hashlib
import json
import re

p = argparse.ArgumentParser()
p.add_argument('generated', type=Path)
p.add_argument('output', type=Path)
p.add_argument('config', type=Path)
p.add_argument('rt64', type=Path)
p.add_argument('--candidate-config', action='store_true')
p.add_argument('--negative-legacy-call', action='store_true')
a = p.parse_args()

actor = None
for path in a.generated.glob('funcs_*.c'):
    match = re.search(r'RECOMP_FUNC void func_80011CC0\(.*?(?=\nRECOMP_FUNC |\Z)',
                      path.read_text(), re.S)
    if match:
        actor = match[0]
        break
assert actor, 'Native actor renderer missing'
hooks = []
for line in a.config.read_text().splitlines():
    if not any(name in line for name in ('rr64_mk64_items_actor_call(', 'rr64_rider_skin_actor_call(')) or line.lstrip().startswith('#'):
        continue
    h = re.fullmatch(r'\s*\{\s*func\s*=\s*"func_(\w+)"\s*,\s*before_vram\s*=\s*'
                     r'(0x[0-9A-Fa-f]+)\s*,\s*text\s*=\s*(".*")\s*\},?\s*', line)
    assert h and h[1] == '80011CC0'
    address = int(h[2], 16)
    assert address in (0x80012028, 0x80012074)
    hook = json.loads(h[3])
    marker = f'    // 0x{address:08X}:'
    before, after = actor.split(marker)
    if not before.rstrip().endswith(hook.strip()):
        assert a.candidate_config, 'Missing generated production actor-call hook'
        # Candidate-only extraction replaces the prior configured material hook.
        before = re.sub(r'\s*#ifdef RR64_EXPERIMENTAL_COURSE\s*ctx->r4 = '
                        r'\(int32_t\)rr64_mk64_items_actor_list\([^;]+;\s*#endif\s*$',
                        '\n', before)
        before += hook + '\n'
    actor = before + marker + after
    hooks.append(dict(address=h[2], text=hook))
assert len(hooks) == 2, 'Expected both native caller hooks'

parts = []
for name, begin, end in [('bike', 0x80012008, 0x80012030),
                         ('rider', 0x8001205C, 0x8001208C)]:
    marker = f'    // 0x{begin:08X}:'
    chunk = marker + actor.split(marker, 1)[1].split(f'    // 0x{end:08X}:', 1)[0]
    assert any(name in chunk for name in ('rr64_mk64_items_actor_call(', 'rr64_rider_skin_actor_call('))
    if a.negative_legacy_call:
        hook = next(h['text'] for h in hooks if
                    int(h['address'], 16) == (0x80012028 if name == 'bike' else 0x80012074))
        assert chunk.count(hook) == 1
        chunk = chunk.replace(hook, '\n#ifdef RR64_EXPERIMENTAL_COURSE\n'
                              'ctx->r4 = (int32_t)rr64_mk64_items_actor_list('
                              'rdram, (unsigned)ctx->r19, (unsigned)ctx->r4);\n#endif\n')
    parts.append((name, chunk))

text = '#include "recomp.h"\n#include "rr64_native.hpp"\n'
for name, chunk in parts:
    text += (f'void fixture_material_call_{name}(unsigned char*rdram,recomp_context*ctx){{\n'
             + chunk + '\nL_800120C8:\nreturn;\n}\n')
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(text)

# Preserve the production resolver, including its extended-address and alignment
# branches. The adapter substitutes only the surrounding State/RSP data layout.
rsp = (a.rt64 / 'src/hle/rt64_rsp.cpp').read_text()
begin = rsp.index('    constexpr uint32_t ExtendedMask =')
end = rsp.index('    void RSP::setSegment', begin)
resolver = rsp[begin:end]
assert all(name in resolver for name in
           ('maskPhysicalAddress', 'fromSegmented(', 'fromSegmentedMasked(', 'fromSegmentedMaskedPD('))
adapter = '''#include <array>
#include <cstdint>
namespace FixtureRT64 {
struct State { struct { bool extendRDRAM = false; } extended; };
struct RSP {
    State *state;
    std::array<uint32_t,16> segments;
    template<uint32_t mask> uint32_t maskPhysicalAddress(uint32_t);
    uint32_t fromSegmented(uint32_t);
    uint32_t fromSegmentedMasked(uint32_t);
    uint32_t fromSegmentedMaskedPD(uint32_t);
};
'''
adapter += resolver + '''}
extern "C" unsigned fixture_material_resolve(unsigned address, const unsigned *segments, bool extended) {
    FixtureRT64::State state;
    state.extended.extendRDRAM = extended;
    FixtureRT64::RSP rsp{&state, {}};
    for(unsigned i=0;i<16;++i) rsp.segments[i]=segments[i];
    return rsp.fromSegmentedMasked(address);
}
'''
a.output.with_suffix('.resolver.cpp').write_text(adapter)
a.output.with_suffix('.json').write_text(json.dumps(dict(
    candidateHooks=a.candidate_config, negativeLegacyCall=a.negative_legacy_call,
    hooks=hooks, callerSlices={name:hashlib.sha256(chunk.encode()).hexdigest() for name,chunk in parts},
    addressResolverSHA256=hashlib.sha256(resolver.encode()).hexdigest(),
    scope='Exact native actor call/cursor stores plus production RT64 address resolver; bounded control-flow parser excludes rasterization'), indent=2)+'\n')
