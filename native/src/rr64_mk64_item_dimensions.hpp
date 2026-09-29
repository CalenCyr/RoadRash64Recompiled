#pragma once
#include "rr64_mk64_item_state.hpp"

namespace rr64::mk64_items {
// Rider-world units, shared by collision, launch/orbit spacing and rendering.
// Shells retain 75% of their previous 2.3-unit linear size. The fake box keeps
// its original proportions at 2.2 units tall; banana size is unchanged.
constexpr float object_radius(Item item) noexcept {
    return item == Item::FakeBox ? 1.1f : item == Item::Banana ? .35f : .8625f;
}
} // namespace rr64::mk64_items
