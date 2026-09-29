#include "rr64_highlight_weapon.hpp"
#include "rr64_highlight_pose.hpp"
#include "rr64_actor_pose.hpp"
#include "rr64_engine_layout.hpp"
#include "librecomp/addresses.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

extern "C" void func_8000F9E8(unsigned char *, recomp_context *);

namespace rr64::highlights {
namespace {
using namespace engine;
constexpr unsigned table = 0x800D43E8, attack_models = 0x800A63C4;
constexpr unsigned graph_stride = 24, pose_stride = 32;
constexpr unsigned scratch_bytes = maximum_weapon_records * (graph_stride + pose_stride) + 32;
struct Record { unsigned source = 0, type = 0, flags = 0; };
struct Template {
    unsigned count = 0, visited_count = 0;
    std::array<Record, maximum_weapon_records> records{};
    std::array<unsigned, maximum_weapon_records * 2> visited{};
    std::uint64_t identity = 14695981039346656037ull;
};
unsigned char *scratch_memory = nullptr;
unsigned char *scratch_allocation = nullptr;
unsigned scratch_address = 0;
thread_local unsigned char *normalized_mapping = nullptr;
thread_local unsigned normalized_record = 0;
thread_local float weapon_scale = 1.f;
thread_local Vec3 weapon_shift{};
thread_local bool weapon_normalized = false;
unsigned word(unsigned char *m, unsigned p) { unsigned v = 0; read_u32(m,p,v); return v; }
unsigned half(unsigned char *m, unsigned p) { std::uint16_t v = 0; read_u16(m,p,v); return v; }
void hash(Template &t, unsigned v) {
    for (unsigned i = 0; i < 4; ++i) { t.identity ^= (v >> (8*i)) & 255u; t.identity *= 1099511628211ull; }
}
// Match 1AD24's source hierarchy and 1ACC8's renderer flags. Only root13,
// child12 transforms, type11 groups and type10 geometry occur in this path.
// Reject nested root13: the native constructor treats it as a new graph.
bool walk(unsigned char *m, unsigned source, unsigned root, unsigned depth, Template &t) {
    if (depth > 16 || t.visited_count == t.visited.size() || (source & 3) ||
        !valid_guest_range(source, 0x58)) return false;
    for (unsigned i=0;i<t.visited_count;++i) if(t.visited[i]==source) return false;
    t.visited[t.visited_count++] = source;
    const unsigned type = word(m,source);
    if (type < 0x10 || type > 0x13 || (type == 0x13 && depth)) return false;
    hash(t, type); hash(t, source - root);
    if (type != 0x11) {
        if (t.count == t.records.size()) return false;
        t.records[t.count++] = {source,type,type == 0x13 ? 0x1000u : type == 0x12 ? 0x2000u : 0u};
    }
    unsigned children = 0, list = 0;
    if (type == 0x13) { children = half(m,source+0xC); list=source+0x40; }
    else if (type == 0x12) {
        children=half(m,source+0x10);
        const std::uint64_t offset=0x58ull+half(m,source+0x18)*24ull+half(m,source+0x20)*16ull;
        if (offset>kRdramSize) return false;
        list=source+static_cast<unsigned>(offset);
    } else if(type==0x11) { children=half(m,source+0xC); list=source+0x38; }
    if (children > maximum_weapon_records || (children && !valid_guest_range(list,children*12))) return false;
    hash(t,children);
    for(unsigned i=0;i<children;++i) {
        const auto p=list+i*12;
        const auto child=std::int64_t(p)+static_cast<std::int32_t>(word(m,p));
        if(child<kRdramBegin || child>kRdramEnd || !walk(m,unsigned(child),root,depth+1,t)) return false;
    }
    if(type==0x12) t.records[t.count-1].flags |= 0x4000;
    return true;
}
bool describe(unsigned char *m, unsigned model, Template &out) {
    // Only resource indices actually referenced by native equipment choices
    // are eligible; a replay cannot index arbitrary memory as an asset table.
    bool known=false;
    for(unsigned i=1;i<=14;++i) known |= word(m,attack_models+i*4)==model;
    if(!known || model>=13) return false;
    const unsigned source=word(m,table+model*4);
    if(!source || word(m,source)!=0x13 || !walk(m,source,source,0,out)) return false;
    return out.count && out.records[0].type==0x13;
}
bool allocate(unsigned char *m) {
    if(scratch_memory==m && scratch_allocation) return true;
    if(scratch_memory) return false; // reset is required when the mapping changes
    auto *p=static_cast<unsigned char *>(recomp::alloc(m,scratch_bytes));
    if(!p) return false;
    const auto offset=p-m;
    if(offset<0 || std::uint64_t(offset)+scratch_bytes>recomp::mem_size || (offset&7)) {
        recomp::free(m,p); return false;
    }
    scratch_memory=m; scratch_allocation=p; scratch_address=0x80000000u+unsigned(offset);
    return true;
}
}

bool capture_weapon(unsigned char *m, unsigned rider, WeaponPose &out) noexcept {
    out={};
    if(!m || !valid_guest_range(rider,engine::rider::stride) || !(half(m,rider+0x5B4)&1)) return false;
    float attack_weight=0;
    const unsigned equipment=word(m,rider+0x564);
    if(!read_float(m,rider+0x558,attack_weight) || !std::isfinite(attack_weight) || attack_weight==0 ||
       static_cast<std::int16_t>(half(m,rider+0x528))<0 || equipment<1 || equipment>14) return false;
    // +5B6 is a per-camera range gate, not the attack visibility decision.
    const unsigned graph=word(m,rider+0x5BC), model=word(m,rider+0x5B8);
    Template source;
    ModelGraphTopologySnapshot live;
    if(word(m,attack_models+equipment*4)!=model || !describe(m,model,source) ||
       !capture_model_graph_topology(m,graph,live) || live.record_count!=source.count || (half(m,graph+0xA)&1)) return false;
    WeaponPose candidate;
    candidate.valid=true; candidate.model=std::uint16_t(model);
    candidate.count=std::uint16_t(source.count); candidate.topology=source.identity;
    for(unsigned i=0;i<source.count;++i) {
        const auto &record=live.records[i];
        if(record.type!=source.records[i].type || word(m,record.address+0x14)!=source.records[i].source ||
           (half(m,record.address+0xA)&0x7000)!=source.records[i].flags) return false;
        candidate.transforms[i][6]=1;
        if(!i || record.type!=0x12) continue; // root comes from recorded rider world state
        const auto pose=word(m,record.address+0xC);
        if((pose&3) || !valid_guest_range(pose,28)) return false;
        for(unsigned j=0;j<7;++j) if(!read_float(m,pose+j*4,candidate.transforms[i][j])) return false;
    }
    if(!valid_weapon_pose(candidate)) return false;
    out=candidate; return true;
}

bool draw_weapon(unsigned char *m, void *context, const WeaponPose &p,
                 const Vec3 &anchor, const Quaternion &rotation, const Vec3 &camera,
                 unsigned bank, float model_scale, const Vec3 &pivot) noexcept {
    auto *rdram=m; // Native MEM_* macros name the mapping explicitly.
    if(!m || !context || !p.valid || !valid_weapon_pose(p) || bank<1 || bank>2 ||
       !std::isfinite(model_scale) || model_scale <= 0 || model_scale > 1) return false;
    Template source;
    if(!describe(m,p.model,source) || source.count!=p.count || source.identity!=p.topology) return false;
    const auto *caller=static_cast<recomp_context *>(context);
    const unsigned stack=unsigned(caller->r29);
    if(stack<kRdramBegin+0x400 || !valid_guest_range(stack-0x400,0x400)) return false;
    const bool normalized=bank==1 && detailed_projection_ready(m);
    float scale=0; double norm=0;
    if(!read_float(m,0x8009DBAC+bank*4,scale) || !std::isfinite(scale) || scale<=0 || scale>1000) return false;
    std::array<float,7> root{};
    for(unsigned i=0;i<3;++i) {
        root[i]=(anchor[i]-camera[i])*scale;
        if(!std::isfinite(anchor[i]) || !std::isfinite(camera[i]) || std::abs(anchor[i])>100000 ||
           std::abs(camera[i])>100000 || !std::isfinite(root[i]) ||
           std::abs(root[i]*(normalized ? 0.1f : 1.f))>30000) return false;
    }
    for(unsigned i=0;i<4;++i) {
        if(!std::isfinite(rotation[i]) || std::abs(rotation[i])>4) return false;
        root[i+3]=rotation[i]; norm+=double(rotation[i])*rotation[i];
    }
    if(norm<1e-12 || norm>16 || !allocate(m)) return false;
    std::memset(scratch_allocation,0,scratch_bytes);
    const unsigned poses=scratch_address+maximum_weapon_records*graph_stride;
    const unsigned root_source=poses+maximum_weapon_records*pose_stride;
    // Only the root source-bank selector is read by F9E8. Clone its header so
    // changing camera units never changes the resident model resource.
    for(unsigned j=0;j<5;++j) MEM_W(j*4,guest_address(root_source))=word(m,source.records[0].source+j*4);
    MEM_H(0x12,guest_address(root_source))=normalized ? 2 : bank;
    for(unsigned i=0;i<p.count;++i) {
        const unsigned record=scratch_address+i*graph_stride, pose=poses+i*pose_stride;
        MEM_H(4,guest_address(record))=source.records[i].type;
        MEM_H(8,guest_address(record))=i+1<p.count ? graph_stride/8 : 0;
        MEM_H(0xA,guest_address(record))=source.records[i].flags;
        MEM_W(0xC,guest_address(record))=pose;
        MEM_W(0x14,guest_address(record))=i ? source.records[i].source : root_source;
        const auto &values=i ? p.transforms[i] : root;
        for(unsigned j=0;j<7;++j) MEM_W(j*4,guest_address(pose))=std::bit_cast<unsigned>(values[j]);
    }
    auto c=*caller;
    c.f_odd=c.mips3_float_mode ? &c.f1.u32l : &c.f0.u32h;
    c.r4=guest_address(scratch_address); c.r5=0;
    // F9E8 consumes this CPU-only graph synchronously into ordinary native
    // matrix/display-list buffers. No scratch address is sent to the RSP.
    struct MatrixScope {
        ~MatrixScope() {
            normalized_mapping=nullptr; normalized_record=0;
            weapon_scale=1; weapon_shift={}; weapon_normalized=false;
        }
    } scope;
    normalized_mapping=m;
    normalized_record=scratch_address;
    weapon_scale=model_scale;
    weapon_normalized=normalized;
    for(unsigned i=0;i<3;++i) {
        weapon_shift[i]=(pivot[i]-anchor[i])*(1-model_scale)*scale;
        if(!std::isfinite(weapon_shift[i])) return false;
    }
    // Record normalization separately from shrink: bank2 still needs its
    // owned root scaled, and skeletal child matrices must remain untouched.
    func_8000F9E8(m,&c);
    return true;
}
void reset_weapon_scratch() noexcept {
    if(scratch_memory && scratch_allocation) recomp::free(scratch_memory,scratch_allocation);
    scratch_memory=nullptr; scratch_allocation=nullptr; scratch_address=0;
}
void normalize_weapon_matrix(unsigned char *m, unsigned record, unsigned address) noexcept {
    if(m!=normalized_mapping || record!=normalized_record || !record || (address&3) ||
       !valid_guest_range(address,64)) return;
    std::array<float,16> matrix{};
    for(unsigned i=0;i<16;++i)
        if(!read_float(m,address+i*4,matrix[i]) || !std::isfinite(matrix[i])) return;
    for(unsigned row=0;row<3;++row)
        for(unsigned col=0;col<3;++col) matrix[row*4+col]*=weapon_scale;
    for(unsigned axis=0;axis<3;++axis) matrix[12+axis]+=weapon_shift[axis];
    for(unsigned row=0;row<4;++row)
        for(unsigned col=0;col<3;++col) {
            auto &value=matrix[row*4+col];
            if(weapon_normalized) value*=0.1f;
            if(!std::isfinite(value) || std::abs(value)>=32768) return;
        }
    for(unsigned row=0;row<4;++row)
        for(unsigned col=0;col<3;++col)
            write_float(m,address+(row*4+col)*4,matrix[row*4+col]);
}
}
extern "C" void rr64_highlights_weapon_matrix(unsigned char *m, unsigned record, unsigned address) {
    rr64::highlights::normalize_weapon_matrix(m,record,address);
}
