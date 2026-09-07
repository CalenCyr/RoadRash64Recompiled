#include "rr64_world_terrain.hpp"
#include "rr64_world_camera.hpp"
#include "rr64_world_render.hpp"
#include "rr64_world_course_regions.hpp"
#include "rr64_actor_render_snapshot.hpp"
#include "librecomp/addresses.hpp"
#include "librecomp/game.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>

namespace rr64::world {
Matrix terrain_matrix(const TerrainCellAsset& cell, float x, float y) noexcept {
    // All 3,289 roots in the supported ROM are identity quaternions. The
    // loader validates that certificate instead of silently dropping rotation.
    return {1,0,0,0, 0,1,0,0, 0,0,0.5f,0,
        cell.authored_origin[0]-x, cell.authored_origin[1]-y, 0, 1};
}
namespace {
using Vector = std::array<float, 4>;
Vector transform(const Vector& v, const Matrix& m) {
    Vector out{};
    for (unsigned c=0; c<4; ++c) for (unsigned r=0; r<4; ++r) out[c] += v[r]*m[r*4+c];
    return out;
}
}
bool terrain_in_frustum(const TerrainCellAsset& cell, const Matrix& model,
    const Matrix& view, const Matrix& projection) noexcept {
    // Conservative AABB/clip-plane test: a cell is culled only when all eight
    // corners lie outside the same plane. Intersecting and enclosing cells stay.
    unsigned outside_all = 63u;
    for (unsigned corner=0; corner<8; ++corner) {
        Vector p{(corner&1)?cell.maximum[0]:cell.minimum[0],
            (corner&2)?cell.maximum[1]:cell.minimum[1],
            (corner&4)?cell.maximum[2]:cell.minimum[2], 1};
        p = transform(transform(transform(p, model), view), projection);
        if (!std::all_of(p.begin(),p.end(),[](float f){return std::isfinite(f);})) return false;
        unsigned mask=0;
        for(unsigned i=0;i<3;++i) {
            // RT64 expands the original 4:3 projection to at most 16:9 in
            // this port (rt64_workload_queue.cpp). Include those side strips
            // even in Original/Stretch mode; the GPU does the final clipping.
            const float limit = p[3] * (i==0u ? 4.0f/3.0f : 1.0f);
            if (p[i] < -limit) mask |= 1u<<(i*2u);
            if (p[i] > limit) mask |= 2u<<(i*2u);
        }
        outside_all &= mask;
    }
    return outside_all==0;
}
namespace {
using namespace rr64::engine;
constexpr unsigned cells_count=4900u, frame_bytes=640u*1024u;
constexpr unsigned command_bytes=288u*1024u;
constexpr unsigned terrain_id_base=0x52510000u;
// Preserve RT64's default component policies, while matching each authored
// cell directly in fixed transform order instead of searching similar draws.
constexpr unsigned matching_group_flags=0x02011555u;
static_assert(cells_count*56u+88u <= command_bytes);
static_assert(cells_count*64u <= frame_bytes-command_bytes);
struct Frame {
    unsigned char* host=nullptr;
    unsigned base=0, assets=0, commands=0, matrices=0;
    unsigned last_epoch=0; bool issued=false;
};
struct Cache {
    unsigned char* mapping=nullptr;
    const unsigned char* rom=nullptr;
    TerrainAssets assets;
    CourseRegions regions;
    CourseRegions::Mask allowed{};
    bool course_scoped=false;
    std::array<Frame,2> frames{};
    bool attempted=false, ready=false, drawing=false;
    unsigned grid=0;
    std::array<bool,cells_count> stock{};
    TerrainStatistics stats{};
    std::mutex mutex;
};
Cache& cache() { static auto c=std::make_unique<Cache>(); return *c; }
void word(unsigned char* rdram,unsigned addr,unsigned value) { MEM_W(0,guest_address(addr))=value; }
void command(unsigned char* m,unsigned& p,unsigned a,unsigned b) { word(m,p,a); word(m,p+4,b); p+=8; }
void release(Cache& c) {
    for(auto& f:c.frames) if(f.host) recomp::free(c.mapping,f.host);
    c.frames={}; c.ready=false;
}
bool initialize(Cache& c,unsigned char* m) {
    const auto rom=recomp::get_rom();
    if(c.mapping!=m || c.rom!=rom.data()) {
        // Mapping replacement destroys its heap. Only free in a still-live
        // mapping; guest race/menu streaming never owns these allocations.
        if(c.mapping==m) release(c);
        c.mapping=m; c.rom=rom.data(); c.frames={}; c.assets={};
        c.attempted=false; c.ready=false; c.stats={};
    }
    if(c.attempted) return c.ready;
    c.attempted=true;
    std::string error;
    if(!build_terrain_assets(rom,c.assets,error,true,true)) {
        std::fprintf(stderr,"[RR64-WORLD] terrain cache refused: %s\n",error.c_str()); return false;
    }
    for(const auto& cell:c.assets.cells) {
        if(cell.root_quaternion!=std::array<float,4>{0,0,0,1} || cell.cell_index>=cells_count) {
            std::fprintf(stderr,"[RR64-WORLD] terrain root certificate refused\n"); return false;
        }
    }
    CourseRegions::Mask occupied{};
    for(const auto& cell:c.assets.cells)occupied[cell.cell_index]=true;
    c.regions.build(occupied);
    const unsigned bytes=(unsigned(c.assets.bytes.size())+63u)&~63u;
    // Per-original-graphics-buffer copies keep animated bindings and matrices
    // immutable while RT64 consumes the other buffer. Never allocate per race.
    for(auto& frame:c.frames) {
        frame.host=static_cast<unsigned char*>(recomp::alloc(m,bytes+frame_bytes));
        if(!frame.host) { release(c); return false; }
        const auto offset=frame.host-m;
        if(offset<0x800000 || std::uint64_t(offset)+bytes+frame_bytes>recomp::mem_size) { release(c); return false; }
        frame.base=0x80000000u+unsigned(offset); frame.assets=frame.base;
        frame.commands=frame.base+bytes; frame.matrices=frame.commands+command_bytes;
        for(unsigned i=0;i<c.assets.bytes.size();++i) m[(unsigned(offset)+i)^3u]=c.assets.bytes[i];
        for(const auto& r:c.assets.relocations) word(m,frame.assets+r.word_offset,frame.assets+r.target_offset);
    }
    c.ready=true; c.stats.cached_cells=unsigned(c.assets.cells.size());
    c.stats.cached_triangles=c.assets.triangles; c.stats.cached_bytes=2u*(bytes+frame_bytes);
    std::fprintf(stderr,"[RR64-WORLD] terrain cache cells=%u triangles=%u bytes=%u triangle-batches=1\n",
        c.stats.cached_cells,c.stats.cached_triangles,c.stats.cached_bytes);
    return true;
}
bool frame_context(unsigned char* m,unsigned& slot,unsigned& graphics_slot,unsigned& epoch,
    unsigned& pointer,Matrix& view,Matrix& projection,float& origin_x,float& origin_y) {
    unsigned width=0, base=0, count=0, active_base=0;
    if(!read_u32(m,globals::terrain_map_width,width)||width!=70u ||
        !read_u32(m,0x8009dbd4u,slot)||slot>1u || !rr64_world_camera_ready(m,0u,slot) ||
        !read_u32(m,0x8009cba4u,graphics_slot)||graphics_slot>1u ||
        !read_u32(m,0x800a1830u,epoch)|| !read_u32(m,0x800ac650u,pointer)||
        !read_u32(m,0x800ac658u+graphics_slot*4u,base)||
        !read_u32(m,0x8009cb90u,active_base)||base!=active_base ||
        !read_u32(m,0x800bc9a0u,count)||(count!=0x4650u&&count!=0x36b0u)) return false;
    const unsigned size=0x140u+count*8u;
    // Keep a 1024-byte tail for the original remainder and final commands.
    if(!valid_guest_range(base,size)||pointer<base+0x148u||pointer>base+size-1040u||(pointer&7u)) return false;
    unsigned reset=0, value=0;
    if(!read_u32(m,pointer-8u,reset)||reset!=0xfa000000u ||
        !read_u32(m,pointer-4u,value)||value!=0u) return false;
    if(!read_float(m,0x800dde80u,origin_x)||!read_float(m,0x800dde84u,origin_y)||
        !std::isfinite(origin_x)||!std::isfinite(origin_y)) return false;
    Matrix4x4Snapshot p{},v{};
    if(!decode_n64_matrix(m,0x800b6568u+slot*0x40u,p)||
        !decode_n64_matrix(m,0x800b6de8u+slot*0x40u,v)) return false;
    projection=p.values; view=v.values;
    return p.values[0]!=0 && p.values[5]!=0 && p.values[11]!=0 && v.values[15]==1;
}
}
TerrainStatistics terrain_statistics() noexcept { auto& c=cache(); std::lock_guard lock(c.mutex); return c.stats; }
void terrain_reset_session() noexcept {
    auto& c=cache(); std::lock_guard lock(c.mutex);
    // init_heap follows the on_init callback and reclaims the old allocation
    // arena. Addresses from the preceding run must never be reused or freed.
    c.mapping=nullptr; c.rom=nullptr; c.frames={}; c.assets={}; c.stock.fill(false);
    c.attempted=false; c.ready=false; c.drawing=false; c.grid=0; c.stats={};
}
}
extern "C" void rr64_world_terrain_begin(unsigned char* m) {
    auto& c=rr64::world::cache(); std::lock_guard lock(c.mutex);
    unsigned epoch=0;rr64::engine::read_u32(m,0x800a1830u,epoch);
    rr64::world::CourseRegions::Mask all{};all.fill(true);
    rr64::world::course_frame().publish(m,epoch,all);
    c.drawing=false;c.course_scoped=false;c.allowed.fill(true);
    c.stock.fill(false); c.stats.stock_cells=0;c.stats.course_excluded_cells=0;c.stats.stock_course_excluded=0;
    if(!rr64_world_distance_enabled()||!rr64::lod::supported_scene(m)) return;
    if(!rr64::engine::read_u32(m,rr64::engine::globals::terrain_cell_grid,c.grid)||
        !rr64::engine::valid_guest_range(c.grid,4900u*16u)) return;
    c.drawing=rr64::world::initialize(c,m);
    unsigned mode=0,pending=0;float x=0,y=0;
    using namespace rr64::engine;
    if(c.drawing&&!c.assets.cells.empty()&&
        read_u32(m,globals::main_mode,mode)&&read_u32(m,globals::pending_mode,pending)&&
        mode>=0x1cu&&mode<=0x1eu&&pending>=0x1cu&&pending<=0x1eu&&
        read_float(m,globals::terrain_camera_position,x)&&read_float(m,globals::terrain_camera_position+4u,y)&&
        std::isfinite(x)&&std::isfinite(y)) {
        // Scope BEFORE stock rendering. Visibility during a jump must never
        // make a disconnected island eligible. Height is deliberately absent.
        double best=std::numeric_limits<double>::infinity();unsigned nearest=0;
        for(const auto& cell:c.assets.cells){
            const double dx=double(cell.authored_origin[0])-x,dy=double(cell.authored_origin[1])-y;
            const double distance=dx*dx+dy*dy;
            if(distance<best){best=distance;nearest=cell.cell_index;}
        }
        rr64::world::CourseRegions::Mask anchor{};anchor[nearest]=true;
        c.allowed=c.regions.select(anchor);c.course_scoped=true;
        for(const auto& cell:c.assets.cells)if(!c.allowed[cell.cell_index])++c.stats.course_excluded_cells;
        rr64::world::course_frame().publish(m,epoch,c.allowed);
    }
}
extern "C" unsigned rr64_world_terrain_stock_state(unsigned char* m,unsigned record,unsigned state) {
    auto& c=rr64::world::cache();std::lock_guard lock(c.mutex);
    if(state!=5u||!c.drawing||!c.course_scoped||c.mapping!=m||record<c.grid||(record-c.grid)%16u)return state;
    const unsigned index=(record-c.grid)/16u;
    if(index>=c.allowed.size()||c.allowed[index])return state;
    ++c.stats.stock_course_excluded;
    return 0u; // Change only the branch input, never the live cell's state.
}
extern "C" void rr64_world_terrain_observe(unsigned char* m,unsigned record) {
    auto& c=rr64::world::cache(); std::lock_guard lock(c.mutex);
    if(!c.drawing||c.mapping!=m||record<c.grid||(record-c.grid)%16u) return;
    const auto index=(record-c.grid)/16u;
    if(index<c.stock.size()&&!c.stock[index]) { c.stock[index]=true; ++c.stats.stock_cells; }
}
extern "C" void rr64_world_terrain_draw(unsigned char* m) {
    using namespace rr64::world;
    auto& c=cache(); std::lock_guard lock(c.mutex);
    if(!c.drawing||c.mapping!=m||!rr64_world_distance_enabled()||!rr64::lod::supported_scene(m)) return;
    c.drawing=false;
    unsigned slot=0,graphics_slot=0,epoch=0,pointer=0; float x=0,y=0; Matrix view{},projection{};
    if(!frame_context(m,slot,graphics_slot,epoch,pointer,view,projection,x,y)) { ++c.stats.refusals; return; }
    const auto& allowed=c.allowed;
    auto& f=c.frames[graphics_slot];
    if(f.issued&&f.last_epoch==epoch) { ++c.stats.refusals; return; }
    unsigned dl=f.commands;
    command(m,dl,0x64000019u,0); command(m,dl,0x6400001bu,0); command(m,dl,0x64000029u,0);
    unsigned n=0,triangles=0;
    for(const auto& cell:c.assets.cells) {
        if(c.stock[cell.cell_index]||!allowed[cell.cell_index]||!cell.triangles) continue;
        const auto matrix=terrain_matrix(cell,x,y);
        if(!terrain_in_frustum(cell,matrix,view,projection)) continue;
        const unsigned address=f.matrices+n*64u;
        for(unsigned i=0;i<16;++i) word(m,address+i*4u,std::bit_cast<unsigned>(matrix[i]));
        // Stable across graphics slots, culling and stock/cache handoffs.
        // IDs derive from the authored cell, never its visible-list position.
        command(m,dl,0x6400000cu,terrain_id_base+cell.cell_index);
        command(m,dl,matching_group_flags,0u);
        // LOAD+PUSH in F3DEX2 encoding (push bit is inverted by the decoder).
        command(m,dl,0x64000030u,2u); command(m,dl,0u,address);
        command(m,dl,0xde000000u,f.assets+cell.display_list_offset);
        command(m,dl,0xd8380002u,0x40u);
        command(m,dl,0x6400000du,1u);
        ++n; triangles+=cell.triangles;
    }
    command(m,dl,0xe7000000u,0); command(m,dl,0x6400002au,0);
    command(m,dl,0x6400001cu,0); command(m,dl,0x6400001au,0);
    command(m,dl,0x6400002cu,0); command(m,dl,0xe0525464u,0x20000000u);
    command(m,dl,0xfa000000u,0); command(m,dl,0xdf000000u,0);
    if(!n) { c.stats.visible_cells=0; c.stats.drawn_triangles=0; return; }
    // Replace the just-written FA reset with a three-command bridge. Exactly
    // 16 bytes are added, inside the certified current original Gfx allocation.
    unsigned bridge=pointer-8u;
    command(m,bridge,0xe0525464u,0x10000064u);
    command(m,bridge,0x6400002cu,1u);
    command(m,bridge,0xde000000u,f.commands);
    word(m,0x800ac650u,bridge);
    f.issued=true; f.last_epoch=epoch;
    c.stats.visible_cells=n; c.stats.drawn_triangles=triangles; ++c.stats.frames;
}


