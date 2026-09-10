// Opt-in statistical command timing. Sampling never changes command dispatch.
// Buckets 0..255 are ordinary opcodes; 256..511 are extended subcommands.
// Interpret ordinary opcodes using the active microcode, not a universal label.
#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
namespace RT64::RR64CommandProfile {
inline bool enabled() {
    static const bool result=[] {const char *v=std::getenv("RR64_COMMAND_PROFILE");return v&&std::strcmp(v,"1")==0;}();
    return result;
}
inline bool select(uint32_t &random) {
    // Fixed independent diagnostic PRNG; avoids periodic alignment with repeated
    // command packets. Never touches game randomness. Approximately 1 in 1024.
    random^=random<<13;random^=random>>17;random^=random<<5;
    return (random&1023u)==0;
}
struct Bucket {std::atomic<uint64_t> samples{0},nanoseconds{0};};
inline std::array<Bucket,512> buckets;
class Scope {
    using Clock=std::chrono::steady_clock;
    unsigned index=0;
    bool active=false;
    Clock::time_point begin{};
public:
    Scope(uint32_t word,uint8_t extendedOpcode) {
        if(!enabled())return;
        thread_local uint32_t random=0x6d2b79f5u;
        if(!select(random))return;
        const unsigned opcode=word>>24;
        index=(extendedOpcode&&opcode==extendedOpcode)?256u+(word&255u):opcode;
        active=true;begin=Clock::now();
    }
    ~Scope() {
        if(!active)return;
        const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-begin).count();
        buckets[index].nanoseconds.fetch_add(uint64_t(ns),std::memory_order_relaxed);
        buckets[index].samples.fetch_add(1,std::memory_order_relaxed);
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
