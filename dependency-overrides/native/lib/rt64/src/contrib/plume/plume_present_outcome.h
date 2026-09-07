#pragma once

#include <cstdint>

namespace plume {
    enum class D3D12PresentOutcome : uint32_t {
        Accepted,
        Busy,
        Occluded,
        FocusDeferred,
        Error,
        Count
    };

    // Accepted means a successful, non-occluded DXGI return, not physical
    // scan-out. FocusDeferred never calls DXGI and carries resultCode zero.
    // The optional observer must not block or alter presentation behavior.
    using D3D12PresentOutcomeCallback = void (*)(D3D12PresentOutcome outcome,
        uint32_t resultCode) noexcept;
    void setD3D12PresentOutcomeCallback(D3D12PresentOutcomeCallback callback) noexcept;
}
