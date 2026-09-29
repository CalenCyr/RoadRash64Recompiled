#pragma once

#include <chrono>
#include <cstdint>
#include <array>

namespace rr64::prediction {
// Copied into the existing asynchronous sync log. No formatting, allocation or
// file I/O occurs during reconciliation. Sequence identifies repeated samples.
enum class ReconcileStage : unsigned {
    History=1, Traffic, Baseline, Replay, Capture, PrepareCommit, Commit, Applied,
    ResourcePending, TrafficPending
};
// Transaction-owned camera controls and actor values only. No render-worker
// eye/projection globals are sampled by the game-thread reconciliation trace.
struct CameraDiagnostic {
    unsigned valid=0,mode=0,mapped_actor=~0u,state_flags=0;
    std::array<float,4> rotation{};
    std::array<float,3> anchor{};
};
struct CameraAuthorityDiagnostic {
    unsigned valid=0,state_flags=0;
    std::array<float,4> rotation{};
};
struct ReconcileDiagnostic {
    unsigned sequence=0,round=0,tick=0,ack=0,pending=0,steps=0,stage=0;
    std::uint64_t us=0;
    std::uint64_t simulated_us=0,history_us=0,replay_us=0;
    float distance=0;
    unsigned camera_view=~0u;
    CameraDiagnostic camera_before{},camera_replayed{},camera_after{};
    CameraAuthorityDiagnostic camera_authority{};
};
inline thread_local ReconcileDiagnostic last_reconcile;
class ReconcileTrace {
    std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now();
public:
    ReconcileDiagnostic value;
    ReconcileTrace(unsigned round,unsigned tick,unsigned ack,unsigned pending) {
        value.sequence=last_reconcile.sequence+1;value.round=round;value.tick=tick;
        value.ack=ack;value.pending=pending;stage(ReconcileStage::History);
    }
    void stage(ReconcileStage s){value.stage=static_cast<unsigned>(s);}
    ~ReconcileTrace(){
        value.us=std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now()-started_).count();
        last_reconcile=value;
    }
};
} // namespace rr64::prediction
