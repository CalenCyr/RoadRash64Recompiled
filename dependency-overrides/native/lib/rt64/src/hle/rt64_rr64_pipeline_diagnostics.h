// Optional observations only: never use these probes to authorize rendering.
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" void rr64_record_pipeline_stage(unsigned int stage,
    unsigned long long nanoseconds);
extern "C" void rr64_record_pipeline_resources(unsigned long long targets,
    unsigned long long targetBytes, unsigned long long batches,
    unsigned long long batchBytes, unsigned long long textures,
    unsigned long long retiredTextures, unsigned long long reuseHits,
    unsigned long long reuseMisses, unsigned long long recycledBytes, unsigned long long recycledImages);

namespace RT64::RR64PipelineDiagnostics {
    inline bool enabled() {
        // Match the application's process-start opt-in. Disabled probes must
        // not read the clock or prepare expensive observation data.
        static const bool value = [] {
            const char* setting = std::getenv("RR64_DIAGNOSTICS");
            return setting && std::strcmp(setting, "1") == 0;
        }();
        return value;
    }

    enum class Stage : unsigned int {
        FullSync, ProducerWaitPresent, PresentWaitProducer, Matching,
        RenderTotal, RenderLock, WorkerLock, UploadWait, SubmitWait,
        GpuCommands, CopyTotal, CopyAllocation, PresentTotal, DisplayList,
        GuestUpdate, FullSyncTiles, FullSyncParameters, FullSyncUpload,
        FullSyncUploadWait, FullSyncGpuWait, FullSyncTextureWait,
        FullSyncAdvance, RspVertices, RspTriangles, RspTriangleBatch, GeometryCollect, GeometryIntern,
        GeometryCertify, GeometryCoherence, MatchScene, GeometryHash, GeometryReuse, Count
    };

    inline bool stageEnabled(Stage stage) {
        if (!enabled()) { return false; }
        if (stage != Stage::RspVertices && stage != Stage::RspTriangles && stage != Stage::RspTriangleBatch) { return true; }
        // Per-packet probes can execute millions of times per report. Keep them
        // separate from ordinary frame-stage timing to avoid skewing CPU tests.
        static const bool packets = [] {
            const char *value = std::getenv("RR64_PACKET_TIMING");
            return value && std::strcmp(value, "1") == 0;
        }();
        return packets;
    }

    inline constexpr const char* Names[] = {
        "full-sync", "producer-wait-present", "present-wait-producer", "matching",
        "render-total", "render-lock", "worker-lock", "upload-wait", "submit-wait",
        "gpu-commands", "copy-total", "copy-allocation", "present-total",
        "display-list", "guest-update", "full-sync-tiles", "full-sync-parameters",
        "full-sync-upload", "full-sync-upload-wait", "full-sync-gpu-wait",
        "full-sync-texture-wait", "full-sync-advance",
        "rsp-vertices", "rsp-triangles-inclusive", "rsp-triangle-batch-inclusive",
        "geometry-collect", "geometry-intern", "geometry-certify", "geometry-coherence",
        "match-scene-inclusive", "geometry-hash", "geometry-reuse-copy"
    };
    static_assert(sizeof(Names) / sizeof(Names[0]) == static_cast<unsigned int>(Stage::Count));

    class Scope {
    public:
        explicit Scope(Stage stage) : stage(stage), active(stageEnabled(stage)),
            start(active ? Clock::now() : Clock::time_point{}) { }
        ~Scope() { finish(); }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        void finish() {
            if (!active) { return; }
            active = false;
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
            if (ns >= 0) {
                rr64_record_pipeline_stage(static_cast<unsigned int>(stage), static_cast<unsigned long long>(ns));
            }
        }
    private:
        using Clock = std::chrono::steady_clock;
        Stage stage;
        bool active;
        Clock::time_point start;
    };
}
