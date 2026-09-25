#include "rr64_online_terrain.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_prediction_rules.hpp"
#ifdef RR64_EXPERIMENTAL_COURSE
#include "rr64_experimental_course.hpp"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <span>
#include <stdexcept>

namespace recomp { std::span<const std::uint8_t> get_rom(); }
extern "C" void func_8001BDF8(unsigned char*, recomp_context*);

namespace {
using namespace rr64::engine;
constexpr unsigned cells = 4900, magic = 0x52525443;
constexpr unsigned descriptor_offset = 32, payload_offset = 48;
struct Entry { unsigned source = 0, bytes = 0; };
struct Bank {
    unsigned char* mapping = nullptr;
    std::span<const std::uint8_t> rom;
    std::array<Entry, cells> entries{};
    unsigned grid = 0, allocation = 0, capacity = 0, header = 0;
};
// Live and replay descriptors retain their own immutable ROM view. Payload and
// cache words live in low guest RAM and belong to each private replay image.
std::atomic<std::shared_ptr<const Bank>> bank;
thread_local std::shared_ptr<const Bank> replay_bank;
thread_local std::uint64_t replay_bank_epoch = 0;

std::shared_ptr<const Bank> current_bank() {
    if (!rr64::prediction::active()) return bank.load(std::memory_order_acquire);
    return replay_bank_epoch == rr64::prediction::replay_epoch ? replay_bank : nullptr;
}

unsigned word(unsigned char* m, unsigned a) {
    unsigned v = 0;
    read_u32(m, a, v);
    return v;
}
unsigned be32(std::span<const std::uint8_t> b, unsigned p) {
    return (unsigned(b[p]) << 24) | (unsigned(b[p+1]) << 16) |
           (unsigned(b[p+2]) << 8) | b[p+3];
}
unsigned be16(std::span<const std::uint8_t> b, unsigned p) {
    return (unsigned(b[p]) << 8) | b[p+1];
}
bool range(std::span<const std::uint8_t> b, unsigned p, unsigned n) {
    return p <= b.size() && n <= b.size() - p;
}
bool enabled(unsigned char* m) {
    const auto rules = rr64::prediction::physics_rules();
#ifdef RR64_EXPERIMENTAL_COURSE
    // Imported offline AI can leave the camera's native streaming window.
    // Physical support must use the same immutable cell source as online
    // racers rather than treating an unloaded graphics cell as empty terrain.
    if (!rules.active && rr64::experimental_course::active())
        return is_live_race_mode(word(m, globals::main_mode));
#endif
    return rules.active && rules.connected && rules.authoritative &&
           (rr64::prediction::active() || rules.phase == rr64::netplay::Phase::Race) &&
           is_live_race_mode(word(m, globals::main_mode));
}
bool valid(const Bank& b, unsigned char* m) {
    return m && (m == b.mapping || rr64::prediction::active()) &&
           valid_guest_range(b.allocation, b.capacity + payload_offset) &&
           word(m, b.allocation) == magic && word(m, b.allocation+4) == b.capacity &&
           word(m, b.allocation+8) == b.grid && word(m, globals::terrain_cell_grid) == b.grid;
}
bool inspect(Bank& b) {
    const unsigned header=b.header;
    const auto rom = b.rom;
    if (rom.size() > 64u*1024u*1024u || !range(rom, header, 24) ||
        be32(rom, header) != 0x3e || be16(rom, header+16) != 1000 ||
        be16(rom, header+18) != cells || rom[header+23] != 70) return false;
    const unsigned textures = be16(rom, header+20), table = header + 24;
    if (!range(rom, table, (textures+cells)*12)) return false;
    for (unsigned i=0; i<cells; ++i) {
        const unsigned entry = table + (textures+i)*12;
        const unsigned offset = be32(rom, entry), size = be32(rom, entry+4);
        if (!offset) { if (size) return false; continue; }
        if (offset > rom.size()-entry || size < 0xf0 || size > 512u*1024u ||
            (size&7) || !range(rom, entry+offset, size)) return false;
        const unsigned source = entry+offset;
        if (be32(rom, source) != 0x3f || be32(rom, source+4) != size ||
            be32(rom, source+16) > size-16) return false;
        b.entries[i] = {source, size};
        b.capacity = std::max(b.capacity, size);
    }
    return b.capacity != 0;
}
// Original1BDF8 has no safe OOM result. Check the complete pool0 chain before
// calling it, with the same eight-byte request rounding as that allocator.
bool can_allocate(unsigned char* m, unsigned bytes) {
    unsigned p = word(m, 0x800bbd00), selected = 0;
    bytes = (bytes+7u)&~7u;
    for (unsigned count=0; count<131072; ++count) {
        if (!valid_guest_range(p, 8) || (p&7)) return false;
        const unsigned size=word(m,p);
        std::uint8_t busy=0,pad=0;
        std::uint16_t alignment=0;
        read_u8(m,p+6,busy);read_u8(m,p+7,pad);read_u16(m,p+4,alignment);
        if (!size) return busy != 0 && selected != 0;
        const unsigned payload=p+8u+pad;
        if (!alignment || alignment>256 || (alignment&(alignment-1)) ||
            pad>=alignment || (payload&(alignment-1)) || (size&7) ||
            !valid_guest_range(p,8u+pad+size)) return false;
        if (!busy && size>=bytes && !selected) selected=p;
        const unsigned next=p+8u+pad+size;
        if (next<=p) return false;
        p=next;
    }
    return false;
}
void clear_caches(unsigned char* m, unsigned query) {
    // 14DE4 can try BOTH the current and previous wheel-query triangle. A
    // reusable source buffer must never leave either chain pointing at a cell
    // copied for another racer. Scalar results (height/normal/surface) survive.
    for (unsigned p=0x1c;p<=0x48;p+=4) write_u32(m,query+p,0);
    write_u16(m,query+0x18,0);write_u16(m,query+0x1a,0);
}
}

namespace rr64::online_terrain {
void reset() noexcept { bank.store({}, std::memory_order_release); }
bool bind_replay(unsigned char* m, std::span<const std::uint8_t> rom) {
    replay_bank.reset();replay_bank_epoch=rr64::prediction::replay_epoch;
    if (!rr64::prediction::active() || !m) return false;
    if (!enabled(m)) return true;
    auto candidate=std::make_shared<Bank>();
    candidate->mapping=m;candidate->rom=rom;candidate->grid=word(m,globals::terrain_cell_grid);
    if (!valid_guest_range(candidate->grid,cells*16)) return false;
    // The allocation header is captured in low RDRAM. Walking that image's
    // native pool makes saved cases independent of today's live allocation.
    unsigned p=word(m,0x800bbd00);
    for (unsigned count=0;count<131072;++count) {
        if (!valid_guest_range(p,8) || (p&7)) return false;
        const unsigned size=word(m,p);
        std::uint8_t busy=0,pad=0;
        std::uint16_t alignment=0;
        read_u8(m,p+6,busy);read_u8(m,p+7,pad);read_u16(m,p+4,alignment);
        if (!size) break;
        const unsigned payload=p+8u+pad;
        if (!alignment || alignment>256 || (alignment&(alignment-1)) ||
            pad>=alignment || (payload&(alignment-1)) || (size&7) ||
            !valid_guest_range(p,8u+pad+size)) return false;
        const unsigned capacity=word(m,payload+4);
        if (busy && capacity && capacity<=512u*1024u && size>=capacity+payload_offset &&
            word(m,payload)==magic &&
            word(m,payload+8)==candidate->grid &&
            word(m,payload+20)==rom.size() &&
            word(m,payload+descriptor_offset)==payload+payload_offset) {
            candidate->header=word(m,payload+16);
            if (!inspect(*candidate) || candidate->capacity!=capacity) return false;
            for (unsigned i=0;i<cells;++i) {
                const auto e=candidate->entries[i];
                std::uint16_t blocks=0;read_u16(m,candidate->grid+i*16+14,blocks);
                if ((e.bytes && word(m,candidate->grid+i*16+4)!=(0xb0000000u|e.source)) ||
                    unsigned(blocks)*8!=e.bytes) return false;
            }
            candidate->allocation=payload;
            // Never trust a copied last-cell cache against a different ROM
            // supplied to a saved case. Reload from this transaction's ROM.
            write_u32(m,payload+12,0xffffffffu);
            replay_bank=std::move(candidate);
            return true;
        }
        p=payload+size;
    }
    // Stock offline images return before this search and retain stock behavior.
    // Online and offline imported captures require their own saved allocation;
    // an old image without it must not borrow live scratch or silently restore
    // camera-dependent floors.
    return false;
}
}

extern "C" int rr64_online_terrain_prepare(unsigned char* m, void* opaque) noexcept(false) {
    if (!m || !opaque || !enabled(m)) return 1;
    const auto current = current_bank();
    if (current && valid(*current,m)) return 1;
    // Historical replay may use only the scratch allocation present when its
    // snapshot was captured. Never allocate from or consult a live heap here.
    if (rr64::prediction::active()) return 0;
    const auto unavailable = []() -> int {
#ifdef RR64_EXPERIMENTAL_COURSE
        // Returning false enters online authority failure handling and skips
        // the update. An offline race has no authority session to fail, so
        // report a course error before actor physics rather than wait forever
        // or continue with silently absent ground.
        if (!rr64::prediction::physics_rules().active && rr64::experimental_course::active())
            throw std::runtime_error("Imported course: collision terrain scratch unavailable");
#endif
        return 0;
    };
    auto prepared = std::make_shared<Bank>();
    prepared->mapping=m;prepared->rom=recomp::get_rom();
    prepared->header=0x18d380;
#ifdef RR64_EXPERIMENTAL_COURSE
    if (rr64::experimental_course::installed())
        prepared->header=rr64::experimental_course::terrain_rom_offset();
#endif
    prepared->grid=word(m,globals::terrain_cell_grid);
    if (!valid_guest_range(prepared->grid,cells*16) || !inspect(*prepared) ||
        !can_allocate(m,prepared->capacity+payload_offset)) return unavailable();
    auto context=*static_cast<recomp_context*>(opaque);
    context.f_odd=&context.f0.u32h;context.r4=0;context.r5=prepared->capacity+payload_offset;
    func_8001BDF8(m,&context);
    prepared->allocation=unsigned(context.r2);
    if (!valid_guest_range(prepared->allocation,prepared->capacity+payload_offset)) return unavailable();
    const unsigned a=prepared->allocation;
    write_u32(m,a,magic);write_u32(m,a+4,prepared->capacity);write_u32(m,a+8,prepared->grid);
    write_u32(m,a+12,0xffffffffu);
    write_u32(m,a+16,prepared->header);write_u32(m,a+20,unsigned(prepared->rom.size()));
    write_u32(m,a+descriptor_offset,a+payload_offset);
    bank.store(std::move(prepared),std::memory_order_release);
    return 1;
}

extern "C" void rr64_online_terrain_query_begin(unsigned char* m, void* opaque) {
    if (!m || !opaque) return;
    const auto b=current_bank();
    if (!b || !valid(*b,m)) return;
    const auto& c=*static_cast<recomp_context*>(opaque);
    const unsigned query=unsigned(c.r4), payload=b->allocation+payload_offset;
    if (valid_guest_range(query,0x6c) &&
        (word(m,query+0x2c)==payload || word(m,query+0x30)==payload)) clear_caches(m,query);
}

extern "C" void rr64_online_terrain_lookup(unsigned char* m, void* opaque) {
    if (!m || !opaque || !enabled(m)) return;
    auto& c=*static_cast<recomp_context*>(opaque);
    if (c.r5==5) return;
    const auto b=current_bank();
    if (!b || !valid(*b,m)) return;
    const unsigned record=unsigned(c.r4),query=unsigned(c.r8);
    if (record<b->grid || (record-b->grid)%16 || (record-b->grid)/16>=cells ||
        !valid_guest_range(query,0x6c)) return;
    const unsigned index=(record-b->grid)/16;
    const auto entry=b->entries[index];
    // A genuinely empty cell stays empty. In-flight renderer state is never
    // changed and its DMA may finish independently after this query.
    if (!entry.bytes || word(m,record+4)!=(0xb0000000u|entry.source)) return;
    const unsigned a=b->allocation,payload=a+payload_offset;
    if (word(m,a+12)!=index) {
        for (unsigned i=0;i<entry.bytes;++i) m[((payload-kRdramBegin)+i)^3u]=b->rom[entry.source+i];
        write_u32(m,a+12,index);
    }
    clear_caches(m,query);
    c.r4=guest_address(a+descriptor_offset);c.r5=5;
}
