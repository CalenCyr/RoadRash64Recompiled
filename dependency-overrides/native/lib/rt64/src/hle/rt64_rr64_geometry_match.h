// Exact geometry correspondence for presentation-only interpolation.
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace RT64::RR64GeometryMatch {
    // RT64 still builds as C++17. Keep the view allocation-free without raising
    // its language requirement to use std::span.
    template <typename T>
    class ArrayView {
    public:
        constexpr ArrayView() = default;
        constexpr ArrayView(const T *values, std::size_t count) : values_(values), count_(count) {}
        template <std::size_t N>
        constexpr ArrayView(const std::array<T, N> &values) : values_(values.data()), count_(N) {}

        constexpr const T *data() const { return values_; }
        constexpr std::size_t size() const { return count_; }
        constexpr bool empty() const { return count_ == 0u; }
        constexpr const T &operator[](std::size_t index) const { return values_[index]; }
        constexpr const T *begin() const { return values_; }
        constexpr const T *end() const { return count_ ? values_ + count_ : values_; }

    private:
        const T *values_ = nullptr;
        std::size_t count_ = 0;
    };

    struct GeometryView {
        // Ordered local XYZ triples for one transform, before its world matrix.
        ArrayView<float> positions;
        // Ordered triangle triples from every relevant draw for that transform.
        // Indices are global within the workload; firstVertex removes relocation.
        ArrayView<uint32_t> triangleIndices;
        uint32_t firstVertex = 0;
    };

    inline bool validGeometry(const GeometryView &geometry) {
        if (!geometry.positions.data() || geometry.positions.empty() || ((geometry.positions.size() % 3u) != 0u) ||
            !geometry.triangleIndices.data() || geometry.triangleIndices.empty() || ((geometry.triangleIndices.size() % 3u) != 0u)) {
            return false;
        }

        const std::size_t vertexCount = geometry.positions.size() / 3u;
        // Check subtraction before addition, including a range ending at the
        // largest representable global index. All subsequent offsets stay valid.
        const uint64_t indexLimit = uint64_t(std::numeric_limits<uint32_t>::max()) + 1u;
        if (uint64_t(vertexCount) > (indexLimit - uint64_t(geometry.firstVertex))) {
            return false;
        }

        for (const float position : geometry.positions) {
            if (!std::isfinite(position)) {
                return false;
            }
        }
        for (const uint32_t index : geometry.triangleIndices) {
            if ((index < geometry.firstVertex) ||
                (uint64_t(index) - uint64_t(geometry.firstVertex) >= uint64_t(vertexCount))) {
                return false;
            }
        }
        return true;
    }

    // Explicitly identified animated meshes can change finite local positions,
    // but must retain their ordered normalized connectivity and vertex count.
    inline bool compatibleTopology(const GeometryView &current, const GeometryView &previous) {
        if ((current.positions.size() != previous.positions.size()) ||
            (current.triangleIndices.size() != previous.triangleIndices.size()) ||
            !validGeometry(current) || !validGeometry(previous)) {
            return false;
        }

        for (std::size_t i = 0; i < current.triangleIndices.size(); i++) {
            if ((current.triangleIndices[i] - current.firstVertex) !=
                (previous.triangleIndices[i] - previous.firstVertex)) {
                return false;
            }
        }
        return true;
    }

    // A matching vertex count is insufficient: streamed meshes and LODs can
    // reuse the same slots. Compare ordered positions and normalized topology
    // directly, avoiding both pointer identity and hash-collision assumptions.
    // This is a necessary geometry check, not a unique object identity: callers
    // must separately reject ambiguous transform/scene correspondences.
    inline bool compatibleGeometry(const GeometryView &current, const GeometryView &previous) {
        if (!compatibleTopology(current, previous)) {
            return false;
        }
        for (std::size_t i = 0; i < current.positions.size(); i++) {
            // Numeric equality intentionally treats signed zero as the same
            // local position. NaNs and infinities were rejected above.
            if (current.positions[i] != previous.positions[i]) {
                return false;
            }
        }
        return true;
    }
}
