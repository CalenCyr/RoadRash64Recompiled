#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace rr64::race_pack {
enum class Compatibility : std::uint8_t { Pending, Ready, Missing, Disabled, Different };
struct Identity {
    std::uint32_t version = 0; // Zero is the original course set.
    std::array<char, 16> pack{};
    std::array<char, 32> course{};
    std::array<std::uint8_t, 32> digest{};
    bool operator==(const Identity &) const = default;
};
inline bool valid(const Identity &id) noexcept {
    if (id.version == 0)
        return id == Identity{};
    if (id.version != 1 || id.pack != std::array<char, 16>{'m', 'k', '6', '4'} ||
        id.course[0] == 0 ||
        std::none_of(id.digest.begin(), id.digest.end(), [](auto n) { return n != 0; }))
        return false;
    bool end = false;
    for (char c : id.course) {
        if (c == 0) {
            end = true;
            continue;
        }
        if (end || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    }
    return end;
}
#ifdef RR64_EXPERIMENTAL_COURSE
Identity selected_identity() noexcept;
Compatibility compatibility(const Identity &identity) noexcept;
bool apply_identity(const Identity &identity) noexcept;
#else
inline Identity selected_identity() noexcept {
    return {};
}
inline bool apply_identity(const Identity &identity) noexcept {
    return identity == Identity{};
}
inline Compatibility compatibility(const Identity &identity) noexcept {
    if (!valid(identity)) return Compatibility::Different;
    return identity == Identity{} ? Compatibility::Ready : Compatibility::Missing;
}
#endif
} // namespace rr64::race_pack
