#include "hle/rt64_rr64_matching_preflight.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <limits>
#include <memory>
#include <regex>
using namespace RT64;
#include "fixtures/rr64_matching_workload_fixture.inc"
static void require(bool value, const char* message) { if(!value) { std::fprintf(stderr,"preflight FAIL: %s\n",message); std::exit(1); } }
static GameFrame frameFor(const WorkloadQueue& q, uint32_t slot) {
    GameFrame frame; frame.matched=false; frame.workloads={slot}; frame.frameMap.workloads.resize(WORKLOAD_QUEUE_SIZE);
    GameScene scene;
    for(uint32_t p=0;p<q.workloads[slot].fbPairs[0].projectionCount;p++) scene.projections.push_back({slot,0,p});
    frame.perspectiveScenes.push_back(scene); return frame;
}
static void match(GameFrame& cur,const GameFrame& prev,WorkloadQueue& q) {
    bool v=false,t=false,l=false;cur.match(nullptr,q,prev,nullptr,v,t,l);
    require(!v&&!t&&!l,"static fixtures must not invoke GPU uploaders");
}
int main() {
    auto q=std::make_unique<WorkloadQueue>();unsigned proven=0,deferred=0;
    for(unsigned a: {1u,2u,7u,32u}) for(unsigned b: {1u,2u,7u,32u}) for(unsigned mode: {0u,3u,4u,10u}) {
        seed(q->workloads[0],a,8,mode);seed(q->workloads[1],b,8,mode);
        auto prev=frameFor(*q,0),cur=frameFor(*q,1);
        auto skip=RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q);
        require(!RR64MatchingPreflight::unequalSingleSceneMembership(false,cur,prev,*q),"ineligible path must always defer");
        match(cur,prev,*q);
        if(skip) { require(!cur.rr64InterpolationCompatible && (cur.rr64GeometryRejectionReasons&4),"preflight must imply actual matcher membership rejection");proven++; }
        else deferred++;
    }
    seed(q->workloads[0],7,8,0);seed(q->workloads[1],8,8,0);
    auto prev=frameFor(*q,0),cur=frameFor(*q,1);
    require(RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q),"different membership must reject");
    // Allocated but unused matrix slots do not belong to the certificate.
    q->workloads[0].drawData.worldTransforms.resize(8);
    require(RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q),"unused matrix allocation must not equalize used membership");
    cur.perspectiveScenes.push_back(cur.perspectiveScenes.front());
    require(!RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q),"multiple scenes must defer");
    cur=frameFor(*q,1);cur.workloads.push_back(0);
    require(!RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q),"multiple workloads must defer");
    cur=frameFor(*q,1);cur.perspectiveScenes[0].projections[0].projectionIndex=999;
    require(!RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q),"unknown projection must defer");
    cur=frameFor(*q,1);q->workloads[1].drawData.faceIndices[0]=UINT32_MAX;
    require(!RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q),"invalid nondegenerate vertex must defer");
    // Whole-scene union: repeated projections must not double count transforms.
    seed(q->workloads[0],8,8,0);seed(q->workloads[1],8,8,4);prev=frameFor(*q,0);cur=frameFor(*q,1);
    require(!RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q),"projection duplicates must form one union");
    // RDP degenerate tests are discarded before even checking their vertex bounds.
    q->workloads[1].drawData.faceIndices[0]=q->workloads[1].drawData.faceIndices[1]=UINT32_MAX;
    require(!RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q),"degenerate test must not change membership");
    seed(q->workloads[0],0,8,0);seed(q->workloads[1],1,8,0);prev=frameFor(*q,0);cur=frameFor(*q,1);
    require(RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q),"empty versus used scene must reject");
    match(cur,prev,*q);require(!cur.rr64InterpolationCompatible&&(cur.rr64GeometryRejectionReasons&4),"empty-scene rejection must agree with full matcher");
    seed(q->workloads[1],0,8,0);cur=frameFor(*q,1);
    require(!RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q),"two empty scenes must defer");
    // Full match -> native rejected frame -> full match: native predecessor IDs
    // have never been built and must be rebuilt before the next valid pairing.
    seed(q->workloads[0],7,8,0);seed(q->workloads[1],7,8,0);
    prev=frameFor(*q,0);auto full=frameFor(*q,1);match(full,prev,*q);
    seed(q->workloads[2],8,8,0);auto native=frameFor(*q,2);
    require(RR64MatchingPreflight::unequalSingleSceneMembership(true,native,full,*q),"sequence middle frame must reject");
    require(!native.matched && q->workloads[2].transformIdMap.empty(),"native predecessor starts without matching arrays or IDs");
    native.rr64InterpolationCompatible=false;native.rr64GeometryRejectionReasons=4;
    seed(q->workloads[3],8,8,0);auto next=frameFor(*q,3);match(next,native,*q);
    require(next.rr64InterpolationCompatible,"valid matching must recover after native rejection");
    for(const auto& mapping:next.frameMap.workloads[3].transforms) require(mapping.mapped,"every explicit transform must recover its predecessor");
    require(!q->workloads[2].transformIdMap.empty(),"native predecessor IDs must be rebuilt");
    // Verify actual queue eligibility and native-state wiring, including the
    // pause/debugger exclusions that matter even when no extra frame is rendered.
    auto sourcePath=std::filesystem::path(__FILE__).parent_path().parent_path()/"lib/rt64/src/hle/rt64_workload_queue.cpp";
    std::ifstream file(sourcePath);require(file.is_open(),"queue source must be available");
    std::string source((std::istreambuf_iterator<char>(file)),{});
    source=std::regex_replace(source,std::regex(R"(//[^\r\n]*|/\*[\s\S]*?\*/)"),"");
    source.erase(std::remove_if(source.begin(),source.end(),[](unsigned char c){return std::isspace(c);}),source.end());
    require(source.find("boolcanRetainWorkload=retainedRacePath&&!workloadConfig.raytracingEnabled&&!workload.paused&&(workload.debuggerRenderer.globalDrawCallIndex<0)&&!curFrame.isDebuggerCameraEnabled(*this)")!=std::string::npos,"preflight eligibility must exclude paused, debugger and ray tracing");
    require(source.find("if(RR64MatchingPreflight::unequalSingleSceneMembership(canRetainWorkload,curFrame,prevFrame,*this)){curFrame.matched=false;curFrame.rr64InterpolationCompatible=false;curFrame.rr64GeometryRejectionReasons=4u;}else{curFrame.match(")!=std::string::npos,"preflight must preserve unmatched native state and full-match fallback");
    seed(q->workloads[0],2000,80,0);seed(q->workloads[1],2001,80,0);prev=frameFor(*q,0);cur=frameFor(*q,1);
    for(bool equal: {false,true}) {
        if(equal) {seed(q->workloads[1],2000,80,0);cur=frameFor(*q,1);}
        std::vector<double> times;unsigned hits=0;
        for(unsigned r=0;r<70;r++) {
            auto start=std::chrono::steady_clock::now();
            hits+=RR64MatchingPreflight::unequalSingleSceneMembership(true,cur,prev,*q);
            if(r>=10)times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        require(hits==(equal?0u:70u),"timed decision must match membership");std::sort(times.begin(),times.end());
        std::printf("preflight equal=%u median_ms=%.6f hits=%u\n",equal,times[times.size()/2],hits);
    }
    std::printf("preflight proven_rejections=%u deferred_oracles=%u native_recovery=1 queue_gate=1 PASS\n",proven,deferred);
}
