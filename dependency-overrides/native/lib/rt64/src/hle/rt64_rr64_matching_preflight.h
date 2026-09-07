// Necessary membership condition for retained race interpolation.
#pragma once

#include "rt64_workload_queue.h"

namespace RT64::RR64MatchingPreflight {
    inline bool countSceneTransforms(const WorkloadQueue &queue, uint32_t workloadIndex,
        const GameScene &scene, size_t &count)
    {
        if ((workloadIndex >= queue.workloads.size()) || scene.projections.empty()) { return false; }
        const Workload &workload = queue.workloads[workloadIndex];
        const DrawData &data = workload.drawData;
        std::vector<uint8_t> seen(data.worldTransforms.size(), 0);
        count = 0;
        for (const auto &indices : scene.projections) {
            if ((indices.workloadIndex != workloadIndex) || (indices.fbPairIndex >= workload.fbPairCount) ||
                (indices.fbPairIndex >= workload.fbPairs.size())) { return false; }
            const auto &pair = workload.fbPairs[indices.fbPairIndex];
            if ((indices.projectionIndex >= pair.projectionCount) ||
                (indices.projectionIndex >= pair.projections.size())) { return false; }
            const auto &projection = pair.projections[indices.projectionIndex];
            if (projection.type != Projection::Type::Perspective) { continue; }
            if (projection.gameCallCount > projection.gameCalls.size()) { return false; }
            for (uint32_t c = 0; c < projection.gameCallCount; c++) {
                const auto &call = projection.gameCalls[c];
                const uint64_t begin = call.meshDesc.faceIndicesStart;
                const uint64_t end = begin + uint64_t(call.callDesc.triangleCount) * 3;
                if (end > data.faceIndices.size()) { return false; }
                for (uint64_t i = begin; i < end; i += 3) {
                    const uint32_t vertices[3] = {data.faceIndices[size_t(i)],
                        data.faceIndices[size_t(i + 1)], data.faceIndices[size_t(i + 2)]};
                    // The full collector ignores these before inspecting indices.
                    if ((vertices[0] == vertices[1]) || (vertices[0] == vertices[2]) ||
                        (vertices[1] == vertices[2])) { continue; }
                    for (uint32_t vertex : vertices) {
                        if (vertex >= data.worldIndices.size()) { return false; }
                        const uint32_t transform = data.worldIndices[vertex];
                        if (transform >= seen.size()) { return false; }
                        if (!seen[transform]) { seen[transform] = 1; count++; }
                    }
                }
            }
        }
        return true;
    }

    inline bool unequalSingleSceneMembership(bool eligibleRetainedRace,
        const GameFrame &current, const GameFrame &previous, const WorkloadQueue &queue)
    {
        // Only used where the queue can otherwise retain moving raster output.
        // Pause/debugger/composition and ray tracing keep the full matching path.
        if (!eligibleRetainedRace || (current.workloads.size() != 1) ||
            (previous.workloads.size() != 1) || (current.perspectiveScenes.size() != 1) ||
            (previous.perspectiveScenes.size() != 1)) { return false; }
        size_t currentCount = 0, previousCount = 0;
        if (!countSceneTransforms(queue, current.workloads.front(), current.perspectiveScenes.front(), currentCount) ||
            !countSceneTransforms(queue, previous.workloads.front(), previous.perspectiveScenes.front(), previousCount)) {
            return false; // Unknown structure must use all existing checks.
        }
        // The full certificate requires a bijection covering both sets. Unequal
        // cardinality proves rejection; equal cardinality proves nothing.
        return currentCount != previousCount;
    }
}
