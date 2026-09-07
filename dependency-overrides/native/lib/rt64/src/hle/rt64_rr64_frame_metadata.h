// Road Rash presentation metadata captured at the State queue boundary.
#pragma once

#include "rt64_rr64_frame_pacing.h"

namespace RT64::RR64FramePacing {
    struct SceneSnapshot {
        uint64_t epoch = 0;
        bool raceActive = false;

        constexpr bool operator==(const SceneSnapshot &rhs) const {
            return (epoch == rhs.epoch) && (raceActive == rhs.raceActive);
        }
    };

    class SceneTracker {
    public:
        constexpr SceneSnapshot observe(bool raceActive) {
            if ((scene.epoch == 0) || (scene.raceActive != raceActive)) {
                scene.epoch++;
                scene.raceActive = raceActive;
            }
            return scene;
        }

    private:
        SceneSnapshot scene;
    };

    struct InterpolationBatchMetadata {
        uint64_t workloadId = 0;
        SceneSnapshot scene;
        PresentationTargetIdentity target;
        uint32_t sourceRate = 0;
        uint32_t targetRate = 0;

        constexpr uint32_t rejectionReasons(uint64_t requestedWorkloadId,
            SceneSnapshot requestedScene, PresentationTargetIdentity requestedTarget,
            uint32_t requestedSourceRate, uint32_t requestedTargetRate) const
        {
            return (((workloadId == 0) || (workloadId != requestedWorkloadId)) ? 1u : 0u) |
                (!(scene == requestedScene) ? 2u : 0u) |
                ((!target.valid() || !(target == requestedTarget)) ? 4u : 0u) |
                ((sourceRate != requestedSourceRate) ? 8u : 0u) |
                ((targetRate != requestedTargetRate) ? 16u : 0u);
        }

        constexpr bool matches(uint64_t requestedWorkloadId,
            SceneSnapshot requestedScene, PresentationTargetIdentity requestedTarget,
            uint32_t requestedSourceRate, uint32_t requestedTargetRate) const
        {
            return rejectionReasons(requestedWorkloadId, requestedScene,
                requestedTarget, requestedSourceRate, requestedTargetRate) == 0u;
        }
    };

    // A generated batch carries the producer's exact resource certificate.
    // The mutable framebuffer flag is only the legacy path's admission rule;
    // rechecking it here would disable a valid history-selected batch.
    constexpr bool admitGeneratedPresentationBatch(bool stablePresentation,
        bool stockFlag, bool debuggerView, uint32_t frameCount, bool metadataMatches)
    {
        return (frameCount > 1u) && (stablePresentation ?
            (!debuggerView && metadataMatches) : stockFlag);
    }

    // Normal queueing of the next picture must not discard this picture's
    // intermediate frames. Only another workload before the same VI boundary
    // replaces an incomplete batch; the next interval waits for this present.
    constexpr bool queuedWorkloadSupersedesBatch(bool queued,
        uint64_t currentPresentId, uint64_t queuedPresentId)
    {
        return queued && (queuedPresentId == currentPresentId);
    }

    // A late present from an older scene must neither authorize its target in
    // the new scene nor erase the new scene's warm-up history.
    class ScenePresentationHistory {
    public:
        constexpr void activate(SceneSnapshot nextScene) {
            if (nextScene.epoch > scene.epoch) {
                scene = nextScene;
                history.clear();
            }
        }

        constexpr void clear() { history.clear(); }

        constexpr void eraseAddress(uint32_t address) {
            history.eraseAddress(address);
        }

        constexpr void record(SceneSnapshot presentedScene,
            PresentationTargetIdentity target)
        {
            if (presentedScene.raceActive && (presentedScene == scene)) {
                history.record(target);
            }
        }

        constexpr bool contains(SceneSnapshot requestedScene,
            PresentationTargetIdentity target) const
        {
            return requestedScene.raceActive && (requestedScene == scene) &&
                history.contains(target);
        }

    private:
        SceneSnapshot scene;
        PresentationTargetHistory history;
    };
}
