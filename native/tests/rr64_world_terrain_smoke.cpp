// Reuse the ROM-free material/packet data builder and command parity helpers.
#define main rr64_asset_smoke_main
#include "rr64_world_terrain_assets_smoke.cpp"
#undef main
#include "rr64_world_terrain.hpp"
#include "rr64_world_frustum.hpp"
#include "rr64_actor_render_fixture.hpp"
#include "librecomp/addresses.hpp"
#include "librecomp/game.hpp"
#include <cstdlib>
#include <chrono>
#include "rr64_local_world_window.hpp"
bool testLocalWindow=true;

extern "C" void func_8007D814(std::uint8_t*, recomp_context*);
extern "C" void guMtxF2L(std::uint8_t*, recomp_context*);
namespace {
std::vector<std::uint8_t> driver_rom;
bool driver_enabled = true, camera_current = true;
std::size_t next_allocation = 0x1000000u;
std::vector<std::pair<std::size_t, std::size_t>> allocations;
unsigned frees = 0;
void pack(std::vector<std::uint8_t>& memory, std::uint32_t output, const rr64::world::Matrix& matrix) {
    for (unsigned i = 0; i < 16u; ++i) { memory_word(memory, 0x80610000u + i * 4u, std::bit_cast<std::uint32_t>(matrix[i])); }
    recomp_context context{}; context.f_odd = &context.f0.u32h;
    context.r4 = guest(0x80610000u); context.r5 = guest(output); context.r29 = guest(0x807ff000u);
    guMtxF2L(memory.data(), &context);
}
void matrix_parity() {
    rr64::world::TerrainCellAsset cell;
    cell.authored_origin = {1200.25f, -3400.5f}; cell.root_quaternion = {0,0,0,1};
    std::vector<std::uint8_t> memory(8u * 1024u * 1024u);
    memory_word(memory, 0x80000de0u, std::bit_cast<std::uint32_t>(1.0f));
    memory_word(memory, 0x800079f0u, std::bit_cast<std::uint32_t>(0.5f));
    memory_word(memory, 0x800dea80u, 0u); memory_word(memory, 0x800dea94u, 0u);
    memory_word(memory, 0x800ddcd8u, 0x80600000u);
    memory_word(memory, 0x80100000u, 0x80110000u);
    memory_word(memory, 0x80110030u, std::bit_cast<std::uint32_t>(1.0f));
    for (const auto origin : {std::array{1000.0f,-3000.0f}, std::array{-12000.5f,15000.25f},
            std::array{30000.0f,-30000.0f}, std::array{1200.25f,-3400.5f}}) {
        memory_word(memory, 0x80110034u, std::bit_cast<std::uint32_t>(cell.authored_origin[0] - origin[0]));
        memory_word(memory, 0x80110038u, std::bit_cast<std::uint32_t>(cell.authored_origin[1] - origin[1]));
        recomp_context context{}; context.f_odd = &context.f0.u32h;
        context.r4 = guest(0x80100000u); context.r29 = guest(0x807ff000u);
        func_8007D814(memory.data(), &context);
        rr64::engine::Matrix4x4Snapshot actual{};
        check(rr64::engine::decode_n64_matrix(memory.data(), 0x80600000u, actual), "original7D814 produced a valid packed terrain matrix");
        check(actual.values == rr64::world::terrain_matrix(cell, origin[0], origin[1]),
            "terrain native matrix equals actual7D814 including Z-basis .5, unscaled XY and W1");
    }
    const auto distant = rr64::world::terrain_matrix(cell, -60000.0f, 60000.0f);
    check(distant[12] == 61200.25f && distant[13] == -63400.5f && distant[10] == 0.5f && distant[15] == 1.0f,
        "extended float matrix preserves distant translation beyond original16.16 integer range");
}
void driver_checks() {
    using namespace rr64::engine;
    rr64::lod::test::Fixture seed;
    auto memory = std::move(seed.live); memory.resize(128u * 1024u * 1024u, 0);
    driver_rom = fixture(4u, 0u);
    const auto* source_rom = driver_rom.data();
    const auto rom_before = driver_rom;
    constexpr std::uint32_t grid = 0x80500000u;
    memory_word(memory, globals::terrain_cell_grid, grid);
    memory_word(memory, globals::terrain_map_width, 70u);
    memory_word(memory, 0x800ac658u, 0x80200000u); memory_word(memory, 0x800ac65cu, 0x80240000u);
    memory_word(memory, 0x800bc9a0u, 0x4650u);
    memory_word(memory, 0x800dde80u, std::bit_cast<std::uint32_t>(1200.0f));
    memory_word(memory, 0x800dde84u, std::bit_cast<std::uint32_t>(-3400.0f));
    const rr64::world::Matrix view{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const rr64::world::Matrix projection{.01f,0,0,0, 0,.01f,0,0, 0,0,.01f,.001f, 0,0,0,1};
    for (unsigned slot = 0; slot < 2u; ++slot) {
        pack(memory, 0x800b6568u + slot * 64u, projection);
        pack(memory, 0x800b6de8u + slot * 64u, view);
    }
    auto setup = [&](unsigned slot, unsigned epoch) {
        memory_word(memory, 0x8009dbd4u, slot); memory_word(memory, 0x8009cba4u, slot);
        memory_word(memory, 0x800a1830u, epoch);
        const auto base = slot ? 0x80240000u : 0x80200000u;
        memory_word(memory, 0x8009cb90u, base); memory_word(memory, 0x800ac650u, base + 0x200u);
        memory_word(memory, base + 0x1f8u, 0xfa000000u); memory_word(memory, base + 0x1fcu, 0u);
    };
    auto extended_word = [&](std::uint32_t address) {
        std::uint32_t value = 0;
        const auto offset = address & 0x7fffffffu;
        check(offset <= memory.size()-4u, "extended cache word is in the mapped allocation");
        if (offset <= memory.size()-4u) { std::memcpy(&value, memory.data()+offset, 4u); }
        return value;
    };
    auto check_tag = [&](std::uint32_t command, unsigned cell_index) {
        check(extended_word(command) == 0x6400000cu &&
            extended_word(command+4u) == 0x52510000u+cell_index,
            "terrain matching ID belongs to authored cell, independent of visible ordinal or buffer");
        check(extended_word(command+8u) == 0x02011555u && extended_word(command+12u) == 0u,
            "terrain group retains RT64 default component policies with fixed linear order");
        check(extended_word(command+16u) == 0x64000030u && extended_word(command+20u) == 2u &&
            extended_word(command+32u) == 0xde000000u &&
            extended_word(command+40u) == 0xd8380002u && extended_word(command+44u) == 0x40u &&
            extended_word(command+48u) == 0x6400000du && extended_word(command+52u) == 1u,
            "terrain group encloses only its root and cell, restoring both matrix and matching stacks");
    };
    setup(0u, 1u); rr64_world_terrain_begin(memory.data());
    check(allocations.size() == 2u, "terrain allocates exactly two persistent graphics-buffer caches");
    if (allocations.size() != 2u) { return; }
    auto allocation_bytes = [&](unsigned slot) {
        const auto [offset, count] = allocations[slot];
        return std::vector<std::uint8_t>(memory.begin() + offset, memory.begin() + offset + count);
    };
    const auto other_before = allocation_bytes(1u);
    rr64_world_terrain_draw(memory.data());
    auto stats = rr64::world::terrain_statistics();
    const char* diagnostic_switch=std::getenv("RR64_COURSE_DIAGNOSTICS");
    const bool diagnostics=diagnostic_switch&&std::string(diagnostic_switch)=="1";
    check(stats.evidence.enabled==diagnostics,"course evidence obeys independent opt-in");
    if(diagnostics)check(stats.evidence.views[0].valid && stats.evidence.views[0].epoch==1 &&
        stats.evidence.views[0].extended==1 && stats.evidence.views[0].triangles==3 &&
        stats.evidence.views[0].extended_last[0]==1 && !stats.evidence.views[1].valid,
        "course evidence matches emitted fixture cell, epoch and view without another view leaking");
    check(stats.frames == 1u && stats.visible_cells == 1u && stats.drawn_triangles == 3u,
        "certified visible cell draws once outside the stock set");
    check(memory_word(memory, 0x800ac650u) == 0x80200210u,
        "terrain bridge adds exactly16 bytes to certified original Gfx arena");
    check(memory_word(memory, 0x802001f8u) == 0xe0525464u &&
        memory_word(memory, 0x80200200u) == 0x6400002cu &&
        memory_word(memory, 0x80200208u) == 0xde000000u,
        "terrain bridge enables extended interpretation and calls bounded cache");
    const auto first_commands = memory_word(memory, 0x8020020cu);
    check_tag(first_commands+24u, 0u);
    check(allocation_bytes(1u) == other_before, "drawing slot0 leaves slot1 cache untouched");
    const auto first_before = allocation_bytes(0u);
    setup(1u, 2u); rr64_world_terrain_begin(memory.data()); rr64_world_terrain_draw(memory.data());
    check_tag(memory_word(memory, 0x8024020cu)+24u, 0u);
    check(rr64::world::terrain_statistics().frames == 2u && allocation_bytes(0u) == first_before,
        "slot1 draw preserves the previously submitted slot0 command/matrix/data bytes");
    const auto second_before = allocation_bytes(1u);
    setup(1u, 2u); rr64_world_terrain_begin(memory.data()); rr64_world_terrain_draw(memory.data());
    check(rr64::world::terrain_statistics().frames == 2u && allocation_bytes(1u) == second_before,
        "duplicate same-slot epoch cannot overwrite its submitted cache");
    setup(0u, 3u); rr64_world_terrain_begin(memory.data());
    rr64_world_terrain_observe(memory.data(), grid); rr64_world_terrain_observe(memory.data(), grid);
    rr64_world_terrain_draw(memory.data());
    check(rr64::world::terrain_statistics().stock_cells == 1u &&
        rr64::world::terrain_statistics().visible_cells == 0u && memory_word(memory, 0x800ac650u) == 0x80200200u,
        "stock cell ownership is deduplicated and never drawn twice");
    if(diagnostics){const auto evidence=rr64::world::terrain_statistics().evidence;
        check(evidence.views[0].stock_union[0]==1 && evidence.views[0].extended_last[0]==0,
            "stock observation retained and last extended bitmap cleared on empty pass");}
    for (unsigned invalid = 0; invalid < 6u; ++invalid) {
        setup(0u, 4u + invalid); camera_current = true; driver_enabled = true;
        memory_word(memory, 0x8009cb90u, 0x80200000u); memory_word(memory, 0x800bc9a0u, 0x4650u);
        switch (invalid) {
        case 0: camera_current = false; break;
        case 1: memory_word(memory, 0x8009cb90u, 0x80240000u); break;
        case 2: memory_word(memory, 0x802001f8u, 0u); break;
        case 3: memory_word(memory, 0x800bc9a0u, 0xffffffffu); break;
        case 4: memory_word(memory, 0x800ac650u, 0x80223400u); break;
        case 5: driver_enabled = false; break;
        }
        const auto before = memory_word(memory, 0x800ac650u);
        const auto frames = rr64::world::terrain_statistics().frames;
        rr64_world_terrain_begin(memory.data()); rr64_world_terrain_draw(memory.data());
        check(memory_word(memory, 0x800ac650u) == before && rr64::world::terrain_statistics().frames == frames,
            "stale camera/foreign Gfx/corrupt footer/capacity/disabled states do not append terrain");
    }
    check(driver_rom.data() == source_rom && driver_rom == rom_before && allocations.size() == 2u,
        "ROM bytes and persistent allocation count stay unchanged across frames and refusals");
    // The runtime resets its heap after on_init even when both host pointers
    // remain unchanged. Poison the old blocks to prove the next session does
    // not recycle stale cache addresses or free the already-reset arena.
    const auto old_allocations = allocations;
    rr64::world::terrain_reset_session();
    check(rr64::world::terrain_statistics().frames == 0u && frees == 0u,
        "session reset clears statistics without freeing the retired heap");
    for (const auto [offset, count] : old_allocations) {
        std::fill(memory.begin() + offset, memory.begin() + offset + count, 0xceu);
    }
    camera_current = true; driver_enabled = true;
    memory_word(memory, 0x800bc9a0u, 0x4650u);
    setup(0u, 1u); rr64_world_terrain_begin(memory.data()); rr64_world_terrain_draw(memory.data());
    check(allocations.size() == 4u && rr64::world::terrain_statistics().frames == 1u,
        "same ROM/mapping restart allocates fresh buffers and accepts its new epoch");
    for (const auto [offset, count] : old_allocations) {
        check(std::all_of(memory.begin() + offset, memory.begin() + offset + count,
                [](std::uint8_t value) { return value == 0xceu; }),
            "restart does not access retired allocation addresses");
    }
    memory_word(memory,rr64::lod::test::Fixture::race_player_count,4u);memory_word(memory,0x8009DB88u,4u);
    testLocalWindow=false;
    const auto priorFrames=rr64::world::terrain_statistics().frames;
    for(unsigned count=2;count<=4;++count)for(unsigned camera=0;camera<count;++camera){
        setup(0,50);memory_word(memory,rr64::lod::test::Fixture::race_player_count,count);
        memory_word(memory,0x8009DB88u,count);memory_word(memory,globals::active_viewport,camera);
        const auto pointer=memory_word(memory,0x800ac650u);
        rr64_world_terrain_begin(memory.data());rr64_world_terrain_draw(memory.data());
        check(rr64::world::terrain_statistics().frames==priorFrames && memory_word(memory,0x800ac650u)==pointer,
            "two to four split screens retain original terrain without extended submissions");
    }
    memory_word(memory,rr64::lod::test::Fixture::race_player_count,1u);memory_word(memory,0x8009DB88u,1u);
    testLocalWindow=true;
    for(unsigned camera=0;camera<4;++camera){
        setup(0,52);memory_word(memory,rr64::lod::test::Fixture::race_player_count,4u);
        memory_word(memory,0x8009DB88u,4u);memory_word(memory,globals::active_viewport,camera);
        pack(memory,0x800b6568u+camera*0x180u,projection);pack(memory,0x800b6de8u+camera*0x180u,view);
        rr64_world_terrain_begin(memory.data());rr64_world_terrain_draw(memory.data());
        check(rr64::world::terrain_statistics().frames==priorFrames+camera+1,"local window admits nearby terrain for each of four split screens");
    }
    testLocalWindow=true;
    memory_word(memory,rr64::lod::test::Fixture::race_player_count,1u);memory_word(memory,0x8009DB88u,1u);
    if(diagnostics){
        const auto generation=rr64::world::terrain_statistics().evidence.generation;
        setup(0,51);memory_word(memory,globals::active_viewport,0u);
        memory_word(memory,globals::multiplayer_game_setup_words[0],123u);
        rr64_world_terrain_begin(memory.data());rr64_world_terrain_draw(memory.data());
        const auto evidence=rr64::world::terrain_statistics().evidence;
        check(evidence.generation==generation+1 && evidence.raw_setup[2]==123 &&
            !evidence.views[1].valid && evidence.views[0].stock_union[0]==0,
            "changed raw setup resets prior view evidence and union");
    }
    memory_word(memory,rr64::lod::test::Fixture::race_player_count,1u);memory_word(memory,0x8009DB88u,1u);memory_word(memory,globals::active_viewport,0u);
    // Cell culling stays conservative at intersecting planes and rejects only
    // wholly outside bounds; the actual camera producer is tested separately.
    rr64::world::TerrainCellAsset cell; cell.minimum = {-1,-1,-1}; cell.maximum = {1,1,1};
    check(rr64::world::terrain_in_frustum(cell, view, view, view), "intersecting clip bounds remain visible");
    cell.minimum = {1.15f,-.1f,-.1f}; cell.maximum = {1.25f,.1f,.1f};
    check(rr64::world::terrain_in_frustum(cell, view, view, view),
        "terrain in the RT64 widescreen side strip is retained");
    cell.minimum = {-1.25f,-.1f,-.1f}; cell.maximum = {-1.15f,.1f,.1f};
    check(rr64::world::terrain_in_frustum(cell, view, view, view),
        "opposite widescreen side strip is retained");
    cell.minimum = {-1,-1,-1}; cell.maximum = {1,1,1};
    auto outside = view; outside[12] = 10;
    check(!rr64::world::terrain_in_frustum(cell, outside, view, view), "wholly outside clip bounds are culled");

    for(unsigned sample=0;sample<1000;++sample){
        auto root=view;root[12]=float(int(sample%31)-15)*.2f;root[13]=float(int(sample%19)-9)*.2f;root[14]=float(int(sample%13)-6)*.2f;
        const bool reference=rr64::world::terrain_in_frustum(cell,root,view,projection);
        const bool fast=rr64::world::WorldFrustum(view,projection).intersects(cell.minimum,cell.maximum,root);
        check(!reference||fast,"plane culling never drops a box retained by the corner reference");
    }
    // The format permits all 4,900 cells, even though the locked ROM occupies
    for(unsigned sample=0;sample<20000;++sample){
        auto camera=view;
        const float angle=float(sample%360)*0.0174532925f;
        camera[0]=std::cos(angle);camera[1]=std::sin(angle);
        camera[4]=-camera[1];camera[5]=camera[0];
        camera[12]=float(int(sample%113)-56)*10.0f;
        cell.minimum={-20,-40,-10};cell.maximum={30,50,80};
        cell.authored_origin={float(int(sample%71)-35)*100,float(int(sample%53)-26)*100};
        const auto root=rr64::world::terrain_matrix(cell,17,-23);
        const rr64::world::WorldFrustum frustum(camera,projection);
        check(frustum.intersects(cell.minimum,cell.maximum,root)==
            frustum.intersectsTerrain(rr64::world::TerrainBounds(cell.minimum,cell.maximum),root[12],root[13]),
            "shared terrain bounds match general matrix culling across camera rotations and translations");
    }
    {const rr64::world::WorldFrustum frustum(view,projection);
        check(!frustum.intersectsTerrain(rr64::world::TerrainBounds({1,0,0},{0,1,1}),0,0),
            "invalid terrain extents are rejected");}
    // The format permits all 4,900 cells, even though the locked ROM occupies
    // fewer. Exercise the actual writer at that bound, then omit the first
    // cell to prove IDs survive visible-list compaction and buffer changes.
    rr64::world::terrain_reset_session(); driver_rom = fixture(4u, 0u);
    for (unsigned index=1u; index<4900u; ++index) {
        const auto entry=refs+(926u+index)*12u;
        put32(driver_rom, entry, static_cast<std::uint32_t>(partition-entry));
        put32(driver_rom, entry+4u, 0x200u);
    }
    setup(0u, 1u); rr64_world_terrain_begin(memory.data()); rr64_world_terrain_draw(memory.data());
    stats=rr64::world::terrain_statistics();
    check(stats.visible_cells == 4900u && stats.drawn_triangles == 14700u,
        "complete supported cell-table bound fits without truncating geometry");
    const auto all_commands=memory_word(memory, 0x8020020cu);
    for (unsigned index=0; index<4900u; ++index) { check_tag(all_commands+24u+index*56u, index); }
    const auto first_matrix=extended_word(all_commands+24u+28u);
    const auto command_end=all_commands+24u+4900u*56u+64u;
    check(command_end <= first_matrix && first_matrix == all_commands+288u*1024u,
        "maximum tagged command stream cannot overlap terrain matrices");
    check(extended_word(command_end-8u) == 0xdf000000u,
        "maximum tagged stream still writes its terminating command");
    const auto [last_offset,last_size]=allocations[allocations.size()-2u];
    check(first_matrix+4900u*64u <= 0x80000000u+last_offset+last_size,
        "all cell matrices remain inside their owned allocation");
    setup(1u, 2u); rr64_world_terrain_begin(memory.data());
    rr64_world_terrain_observe(memory.data(), grid); rr64_world_terrain_draw(memory.data());
    check(rr64::world::terrain_statistics().visible_cells == 4899u,
        "stock-owned first cell is excluded without changing remaining visibility");
    const auto compact_commands=memory_word(memory, 0x8024020cu);
    for (unsigned index=1u; index<4900u; ++index) { check_tag(compact_commands+24u+(index-1u)*56u, index); }
    rr64::world::terrain_reset_session();driver_rom=fixture(4u,0u);
    const auto isolated_entry=refs+(926u+700u)*12u;
    put32(driver_rom,isolated_entry,static_cast<unsigned>(partition-isolated_entry));
    put32(driver_rom,isolated_entry+4u,0x200u);
    memory_word(memory,globals::main_mode,28u);memory_word(memory,globals::pending_mode,28u);
    setup(0,40);rr64_world_terrain_begin(memory.data());
    const auto before_gate=memory;
    check(rr64_world_terrain_stock_state(memory.data(),grid,5u)==5u&&
        rr64_world_terrain_stock_state(memory.data(),grid+700u*16u,5u)==0u&&
        rr64_world_terrain_stock_state(memory.data(),grid+700u*16u,3u)==3u&&
        rr64_world_terrain_stock_state(memory.data(),grid+1u,5u)==5u,
        "stock course gate preserves selected cells, other lifecycle states and unknown addresses");
    check(before_gate==memory,"stock course gate never changes guest memory or cell lifecycle");
    rr64_world_terrain_observe(memory.data(),grid);
    rr64_world_terrain_draw(memory.data());
    check(rr64::world::terrain_statistics().course_excluded_cells==1&&rr64::world::terrain_statistics().visible_cells==0,
        "lap driver excludes disconnected mainland while leaving the stock course untouched");
    setup(1,41);rr64_world_terrain_begin(memory.data());rr64_world_terrain_draw(memory.data());
    check(rr64::world::terrain_statistics().course_excluded_cells==1&&rr64::world::terrain_statistics().visible_cells==1,
        "looking away with no stock terrain still draws only the nearest complete island");
    setup(0,42);rr64_world_terrain_begin(memory.data());rr64_world_terrain_observe(memory.data(),grid+700u*16u);
    rr64_world_terrain_draw(memory.data());
    check(rr64::world::terrain_statistics().course_excluded_cells==1&&rr64::world::terrain_statistics().visible_cells==1,
        "seeing a disconnected stock island cannot expand the selected course during a jump");
    memory_word(memory,globals::main_mode,23u);memory_word(memory,globals::pending_mode,23u);
    setup(1,43);rr64_world_terrain_begin(memory.data());rr64_world_terrain_draw(memory.data());
    check(rr64_world_terrain_stock_state(memory.data(),grid+700u*16u,5u)==5u,"Big Game stock terrain remains unrestricted");
    check(rr64::world::terrain_statistics().course_excluded_cells==0&&rr64::world::terrain_statistics().visible_cells==2,
        "Big Game keeps all previously visible terrain and clears lap selection");
}
} // namespace

namespace recomp {
std::span<const std::uint8_t> get_rom() { return driver_rom; }
void* alloc(std::uint8_t* rdram, std::size_t size) {
    next_allocation = (next_allocation + 63u) & ~std::size_t(63u);
    if (size > 128u * 1024u * 1024u - next_allocation) { return nullptr; }
    auto* result = rdram + next_allocation; allocations.emplace_back(next_allocation, size);
    next_allocation += size; return result;
}
void free(std::uint8_t*, void*) { ++frees; }
}
extern "C" int rr64_world_distance_enabled() { return driver_enabled; }
extern "C" int rr64_world_camera_ready(unsigned char*, unsigned source, unsigned slot) {
    return camera_current && source == 0u && slot < 2u;
}
#include "rr64_world_course_regions.hpp"
void course_checks(){
    using R=rr64::world::CourseRegions;
    R regions;R::Mask occupied{},stock{};
    // A lap island with a diagonal continuation, and a separate mainland.
    for(unsigned i:{71u,72u,143u,700u,701u})occupied[i]=true;
    regions.build(occupied);stock[71]=true;
    auto selected=regions.select(stock);
    check(selected[71]&&selected[72]&&selected[143]&&!selected[700]&&!selected[701],"whole island retained, mainland excluded, diagonal terrain retained");
    check(selected[0],"unclassified object cells retained conservatively");
    stock[700]=true;selected=regions.select(stock);
    check(selected[143]&&selected[701],"multiple stock-visible islands retained");
    stock={};selected=regions.select(stock);
    check(selected[71]&&selected[701],"no anchor fails open");
    stock[700]=true;selected=regions.select(stock);
    rr64::world::CourseFrame frame;unsigned char memory=0,other=0;
    frame.publish(&memory,4,selected);
    check(!frame.read(&memory,4)[71]&&frame.read(&memory,5)[71]&&frame.read(&other,4)[71],"course selection cannot leak into another frame or mapping");
}
int main() {
    if(std::getenv("RR64_BOUNDS_BENCHMARK")){
        std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        rr64::world::WorldFrustum frustum(identity,identity);
        std::vector<rr64::world::Matrix> matrices;
        std::array<float,3> low{-1,-1,-1},high{1,1,1};
        const rr64::world::TerrainBounds bounds(low,high);
        for(unsigned i=0;i<4900;++i){auto m=identity;m[10]=.5f;
            m[12]=float(int(i%70)-35)*.1f;m[13]=float(int(i/70)-35)*.1f;matrices.push_back(m);}
        for(unsigned run=0;run<6;++run)for(unsigned order=0;order<2;++order){
            const bool fast=(order+run)%2;unsigned hits=0;
            const auto start=std::chrono::steady_clock::now();
            for(unsigned repeat=0;repeat<200;++repeat)for(const auto& m:matrices)
                hits+=fast?frustum.intersectsTerrain(bounds,m[12],m[13]):frustum.intersects(low,high,m);
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            std::printf("bounds-benchmark run=%u specialized=%u ms=%.3f hits=%u\n",run,unsigned(fast),ms,hits);
        }
    }
    {using namespace rr64::world;
        const std::array<float,16> camera{0,1,0,0,-1,0,0,0,0,0,1,0,20,-10,0,1};
        double x=0,y=0;check(terrain_eye(camera,x,y)&&x==10&&y==20,"camera eye inversion follows rotation and translation");
        check(window_cell(0,0,5999,0,6001,1,false),"entry uses nearest box edge");
        check(!window_cell(0,0,6500,0,6501,1,false)&&window_cell(0,0,6500,0,6501,1,true),"hysteresis retains an entered cell without admitting a new distant cell");
        check(!window_cell(0,0,7501,0,7502,1,true),"outer boundary bounds retained terrain");
        check(!window_cell(0,0,0,0,1,1,false,0),"zero slider adds no terrain");
        check(!window_cell(0,0,7000,0,7001,1,false,6000)&&window_cell(0,0,7000,0,7001,1,false,8000),"slider increases admission distance");
        check(!window_cell(0,0,10001,0,10002,1,true,8000),"retention stays bounded at adjusted distance");
    }
    course_checks();
    matrix_parity(); driver_checks();
    std::printf("Terrain driver smoke: %s (%d failures); actual7D814,frustum,privatecache,slot lifetime,Gfx guards\n",
        failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}

extern "C" bool rr64_draw_distance_enabled(){return testLocalWindow;}
extern "C" double rr64_draw_distance_percent(){return 100.0;}
