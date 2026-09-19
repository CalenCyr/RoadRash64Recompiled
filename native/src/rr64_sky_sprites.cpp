#include "recomp.h"
#include "rr64_native.hpp"
#include <cstdint>
#include <cstring>

namespace {
constexpr uint32_t queue_base = 0x800BCAD8u;
constexpr uint32_t record_bytes = 76;
constexpr uint32_t capacity = 96;
struct Span {
    unsigned char *owner = nullptr;
    uint32_t buffer = 2, first = 0;
};
thread_local Span sky;
int64_t guest(uint32_t address) { return static_cast<int32_t>(address); }
uint32_t buffer_index(unsigned char *rdram) { return MEM_W(0, guest(0x8009CBA4)); }
uint32_t count(unsigned char *rdram, uint32_t b) { return MEM_HU(0, guest(0x800BC9D0 + b * 2)); }
} // namespace

// The common sky producer is used by races and attract mode, including each
// local/online camera. Keep its camera/state calculations, but discard only its
// cloud sprites. The native background clear supplies the plain sky color.
// No texture allocation, display-list patching or renderer shader is required.
extern "C" void rr64_sky_queue_begin(unsigned char *rdram) {
    sky = {rdram, buffer_index(rdram), 0};
    sky.first = sky.buffer < 2 ? count(rdram, sky.buffer) : capacity;
}
extern "C" void rr64_sky_queue_end(unsigned char *rdram) {
    const Span span = sky;
    sky = {};
    if (span.owner != rdram || span.buffer >= 2 || buffer_index(rdram) != span.buffer)
        return;
    const uint32_t end = count(rdram, span.buffer);
    if (end > capacity || span.first > end)
        return;
    uint32_t write = span.first;
    for (uint32_t read = span.first; read < end; ++read) {
        const uint32_t source = queue_base + (span.buffer * capacity + read) * record_bytes;
        const uint32_t id = MEM_HU(0x10, guest(source));
        if (id >= 0x2D && id <= 0x3C)
            continue;
        // Preserve any non-cloud record if the producer ever queues one.
        // Records are aligned, whole words, so raw copies preserve guest layout.
        if (write != read) {
            const uint32_t target = queue_base + (span.buffer * capacity + write) * record_bytes;
            std::memmove(rdram + (target - 0x80000000u), rdram + (source - 0x80000000u),
                         record_bytes);
        }
        ++write;
    }
    MEM_H(0, guest(0x800BC9D0 + span.buffer * 2)) = write;
}
