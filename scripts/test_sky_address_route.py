"""Compile the actual RT64 texture handlers in a minimal offline address fixture.

No game/ROM or renderer window is needed. Only State bookkeeping is stubbed;
command decoding, segment conversion, and physical masking come from production.
This catches testing the raw RDP handler while F3DEX2 dispatches elsewhere.
"""
from pathlib import Path
import argparse
import subprocess

root = Path(__file__).resolve().parents[1]
rt = root / 'native/lib/rt64/src'

def function(path, signature):
    text = (rt / path).read_text()
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def generate():
    assert 'gbi->map[G_SETTIMG] = &GBI_F3D::setTextureImage;' in (rt/'gbi/rt64_gbi_f3dex2.cpp').read_text()
    prefix = r'''
#include <cstdint>
#include <cstdio>
namespace RT64 {
constexpr uint32_t ExtendedMask = 0x80000000u;
constexpr uint32_t RDP_ADDRESS_MASK = 0x00FFFFFFu;
enum class DrawAttribute { Texture };
struct State;
struct RDP {
    State* state;
    struct { uint8_t fmt, siz; uint16_t width; uint32_t address; } texture{};
    uint32_t maskAddress(uint32_t);
    void setTextureImage(uint8_t, uint8_t, uint16_t, uint32_t);
};
struct RSP {
    State* state;
    uint32_t segments[16]{};
    uint32_t fromSegmented(uint32_t);
    void setTextureImage(uint8_t, uint8_t, uint16_t, uint32_t);
};
struct State {
    struct { bool extendRDRAM = false; } extended;
    RDP* rdp;
    RSP* rsp;
    unsigned updates = 0;
    void updateDrawStatusAttribute(DrawAttribute) { ++updates; }
};
struct DisplayList {
    uint32_t w0, w1;
    uint32_t p0(unsigned shift, unsigned bits) const { return (w0 >> shift) & ((1u << bits)-1); }
};
'''
    bodies = '\n'.join([
        function('hle/rt64_rdp.cpp', 'uint32_t RDP::maskAddress('),
        function('hle/rt64_rdp.cpp', 'void RDP::setTextureImage('),
        function('hle/rt64_rsp.cpp', 'uint32_t RSP::fromSegmented('),
        function('hle/rt64_rsp.cpp', 'void RSP::setTextureImage('),
        'namespace GBI_RDP { ' + function('gbi/rt64_gbi_rdp.cpp', 'void setTextureImage(') + ' }',
        'namespace GBI_F3D { ' + function('gbi/rt64_gbi_f3d.cpp', 'void setTextureImage(') + ' }',
    ])
    suffix = r'''
}
int main() {
    using namespace RT64;
    State state{}; RDP rdp{&state}; RSP rsp{&state}; state.rdp=&rdp; state.rsp=&rsp;
    unsigned failures=0, cases=0;
    // Poison the segment which incorrectly handles 0x81...... sky addresses.
    rsp.segments[1]=0x00300000;
    auto check=[&](uint32_t command, uint32_t pointer, uint32_t expected, bool extended, bool raw) {
        ++cases;
        state.extended.extendRDRAM=extended;
        DisplayList list{command,pointer}; auto* dl=&list;
        auto updates=state.updates;
        if(raw) GBI_RDP::setTextureImage(&state,&dl); else GBI_F3D::setTextureImage(&state,&dl);
        if(rdp.texture.address!=expected || state.extended.extendRDRAM!=extended || state.updates!=updates+1 ||
           rdp.texture.width!=1 || rdp.texture.siz!=2 || rdp.texture.fmt!=((command>>21)&7)) {
            ++failures;
            std::printf("FAIL %08x %08x -> %08x, expected %08x\n",command,pointer,rdp.texture.address,expected);
        }
    };
    for(bool extended : {false,true}) {
        for(bool raw : {false,true}) {
            check(0xFD140000,0x81000200,0x01000200,extended,raw);
            for(unsigned tile=0;tile<16;++tile)
                for(unsigned strip=0;strip<4;++strip) {
                    uint32_t p=0x81000400+tile*4000+strip*1280;
                    check(0xFD540000,p,p-0x80000000,extended,raw);
                }
        }
    }
    // Unmarked HUD/world commands must retain ordinary segmented addressing.
    check(0xFD500000,0x01000400,0x00300400,false,false);
    check(0xFD100000,0x80300200,0x00300200,false,false);
    check(0xFD500000,0x81000400,0x01000400,true,false);
    std::printf("Actual F3D/RDP sky address routes: %u cases, %u failures\n",cases,failures);
    return failures ? 1 : 0;
}
'''
    return '#include <initializer_list>\n' + prefix + bodies + suffix

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args=parser.parse_args()
    out=args.output.resolve(); out.mkdir(parents=True,exist_ok=True)
    (out/'fixture.cpp').write_text(generate())
    (out/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(SkyAddressRoute LANGUAGES CXX)\nadd_executable(SkyAddressRoute fixture.cpp)\ntarget_compile_features(SkyAddressRoute PRIVATE cxx_std_17)\n')
    subprocess.run(['cmake','-S',str(out),'-B',str(out/'build')],check=True)
    subprocess.run(['cmake','--build',str(out/'build'),'--config','Release'],check=True)
    exe=out/'build/Release/SkyAddressRoute.exe'
    if not exe.exists(): exe=out/'build/SkyAddressRoute'
    subprocess.run([str(exe)],check=True)
