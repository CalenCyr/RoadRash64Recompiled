// Bounded, opt-in observations. Producers never format text or perform file I/O.
// Slots are published once and never recycled, so delayed readers cannot observe
// overwritten records. Independent category budgets keep visibility churn from
// exhausting translation evidence. This data never controls rendering.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
namespace RT64::RR64MatchingEvidence {
constexpr unsigned Categories = 3, Capacity = 64;
struct Record {
    uint64_t submission=0, workload=0, topologyHash=0;
    uint32_t category=0, world=UINT32_MAX, previousWorld=UINT32_MAX;
    uint32_t framebuffer=0, projection=0, view=UINT32_MAX;
    uint32_t id=UINT32_MAX, previousId=UINT32_MAX, idOccurrences=0, previousIdOccurrences=0;
    uint32_t positionPolicy=0, ordering=0, vertices=0, indices=0;
    bool mapped=false, worldLerp=false, viewLerp=false;
    std::array<float,3> worldBefore{},worldAfter{},viewBefore{},viewAfter{};
};
struct Slot { Record record; std::atomic<bool> ready{false}; };
struct Buffer {
    std::array<std::array<Slot,Capacity>,Categories> slots{};
    std::array<std::atomic<unsigned>,Categories> attempts{}, reserved{};
    std::array<unsigned,Categories> drained{}; // Single periodic log consumer.
    int claim(unsigned category) {
        if(category>=Categories || reserved[category].load(std::memory_order_relaxed)>=Capacity) return -1;
        unsigned n=attempts[category].fetch_add(1,std::memory_order_relaxed);
        if(n>=8 && n%128!=0) return -1;
        unsigned index=reserved[category].fetch_add(1,std::memory_order_relaxed);
        return index<Capacity ? int(index) : -1;
    }
    void publish(unsigned category,unsigned index,const Record &record) {
        slots[category][index].record=record;
        slots[category][index].ready.store(true,std::memory_order_release);
    }
    template<class Consumer> void drain(Consumer consume) {
        for(unsigned c=0;c<Categories;c++) {
            while(drained[c]<Capacity && slots[c][drained[c]].ready.load(std::memory_order_acquire)) {
                consume(slots[c][drained[c]].record); ++drained[c];
            }
        }
    }
};
inline Buffer buffer;
inline bool enabled() {
    static const bool value=[] { const char *p=std::getenv("RR64_MATCH_EVIDENCE"); return p && std::strcmp(p,"1")==0; }();
    return value;
}
}
