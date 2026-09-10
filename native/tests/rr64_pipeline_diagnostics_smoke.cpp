#include "hle/rt64_rr64_pipeline_diagnostics.h"
#include "hle/rt64_rr64_matching_evidence.h"
#include "hle/rt64_rr64_command_profile.h"
#include <cstdio>
#include <cstring>

static unsigned calls = 0;
static unsigned invalid = 0;
extern "C" void rr64_record_pipeline_stage(unsigned stage, unsigned long long) {
    ++calls;
    if (stage >= static_cast<unsigned>(RT64::RR64PipelineDiagnostics::Stage::Count)) ++invalid;
}

int main(int argc, char** argv) {
    using namespace RT64::RR64PipelineDiagnostics;
    if (argc < 2 || argc > 3) return 2;
    const bool expected = std::strcmp(argv[1], "enabled") == 0;
    if (enabled() != expected) return 3;
    {
        Scope outer(Stage::FullSync);
        { Scope inner(Stage::Matching); }
        if (calls != (expected ? 1u : 0u)) return 4;
        outer.finish();
        outer.finish(); // Explicit finish followed by destruction records once.
    }
    if (calls != (expected ? 2u : 0u) || invalid) return 5;
    const bool packets=argc>2 && std::strcmp(argv[2],"packets")==0;
    for (unsigned i = 0; i < static_cast<unsigned>(Stage::Count); ++i) {
        Scope scope(static_cast<Stage>(i));
        if (!Names[i] || !Names[i][0]) return 6;
    }
    if (calls != (expected ? 2u + static_cast<unsigned>(Stage::Count) - (packets ? 0u : 3u) : 0u) || invalid) return 7;
    std::printf("diagnostics_enabled=%u callbacks=%u nested_and_explicit_finish=passed\n", expected, calls);
    // A delayed producer must not expose incomplete records or block a different
    // category. Published records are consumed once; full buffers never overwrite.
    RT64::RR64MatchingEvidence::Buffer evidence;
    RT64::RR64MatchingEvidence::Record record;
    const int first=evidence.claim(0),second=evidence.claim(0),other=evidence.claim(1);
    if(first!=0 || second!=1 || other!=0) return 8;
    record.world=22; evidence.publish(0,second,record);
    record.world=33; evidence.publish(1,other,record);
    unsigned consumed=0;
    evidence.drain([&](const auto &r){if(r.world!=33) ++invalid; ++consumed;});
    if(consumed!=1 || invalid) return 9;
    record.world=11; evidence.publish(0,first,record);
    evidence.drain([&](const auto &r){if(r.world!=11 && r.world!=22) ++invalid; ++consumed;});
    evidence.drain([&](const auto &){++invalid;});
    if(consumed!=3 || invalid) return 10;
    for(unsigned i=0;i<10000;i++) {
        int slot=evidence.claim(2);
        if(slot>=0){record.world=unsigned(slot);evidence.publish(2,unsigned(slot),record);}
    }
    unsigned count=0;
    evidence.drain([&](const auto &r){if(r.world!=count++) ++invalid;});
    if(count!=RT64::RR64MatchingEvidence::Capacity || invalid || evidence.claim(2)!=-1) return 11;
    std::puts("PASS bounded matching evidence: independent budgets, delayed publication, no overwrite, single consumption");
    // Validate sampling frequency without relying on wall-clock timing values.
    uint32_t random=0x6d2b79f5u;unsigned selected=0;
    for(unsigned i=0;i<1048576;i++)selected+=RT64::RR64CommandProfile::select(random);
    if(selected<800 || selected>1250)return 12;
    const bool commandTiming=RT64::RR64CommandProfile::enabled();
    for(unsigned i=0;i<32768;i++) {
        RT64::RR64CommandProfile::Scope ordinary(0xda000000,0x64);
        RT64::RR64CommandProfile::Scope extended(0x64000030,0x64);
    }
    for(unsigned i=0;i<512;i++) {
        auto count=RT64::RR64CommandProfile::buckets[i].samples.load();
        if(commandTiming && (i==0xda || i==0x130)) {if(!count)return 13;}
        else if(count)return 14;
    }
    std::printf("PASS command sampler enabled=%u distribution=%u per 1048576, bucket routing correct\n",commandTiming,selected);
    return 0;
}
