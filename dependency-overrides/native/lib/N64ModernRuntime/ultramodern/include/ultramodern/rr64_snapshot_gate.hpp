#pragma once
#include <shared_mutex>
#include <mutex>
#include <cstring>
#include <cstddef>
#include <cstdint>
namespace ultramodern::rr64 {
// GPU command parsing and RSP audio tasks may overlap each other. A snapshot
// briefly excludes both, and copies to caller-owned storage without callbacks.
class SnapshotGate {
    std::shared_mutex mutex_;
public:
    auto worker() { return std::shared_lock<std::shared_mutex>(mutex_); }
    bool copy(const unsigned char *source,unsigned char *destination,std::size_t size) {
        if(!source || !destination || !size)return false;
        auto from=reinterpret_cast<std::uintptr_t>(source),to=reinterpret_cast<std::uintptr_t>(destination);
        if(from<to ? to-from<size : from-to<size)return false;
        std::unique_lock lock(mutex_);
        std::memcpy(destination,source,size);
        return true;
    }
};
// Only the currently running emulated CPU thread may call this at an update
// boundary. Never restore this image wholesale into live RDRAM.
bool copy_guest_snapshot(unsigned char *rdram,unsigned char *destination,std::size_t size);
}
