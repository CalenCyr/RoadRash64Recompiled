// A replay camera cut is an authored scene boundary, not physical movement.
#pragma once

#include "rt64_workload_queue.h"

namespace RT64::RR64CameraBoundary {
    struct Phase {
        uint32_t id = 0;
        bool inconsistent = false;
    };

    inline Phase phase(const WorkloadQueue &queue, const GameFrame &frame) {
        Phase result;
        for (const auto &scene : frame.perspectiveScenes) {
            for (const auto &indices : scene.projections) {
                const auto &workload = queue.workloads[indices.workloadIndex];
                const auto &projection = workload.fbPairs[indices.fbPairIndex].projections[indices.projectionIndex];
                const auto &data = workload.drawData;
                if (projection.transformsIndex >= data.viewProjTransformGroups.size()) continue;
                const auto group = data.viewProjTransformGroups[projection.transformsIndex];
                if (group >= data.transformGroups.size()) continue;
                const auto id = data.transformGroups[group].matrixId;
                if ((id & 0xffff0000u) != 0x484c0000u) continue;
                result.inconsistent |= result.id != 0 && result.id != id;
                result.id = id;
            }
        }
        return result;
    }

    inline bool changed(const WorkloadQueue &queue, const GameFrame &current, const GameFrame &previous) {
        const auto cur = phase(queue, current), prev = phase(queue, previous);
        return cur.inconsistent || prev.inconsistent || cur.id != prev.id;
    }
}
