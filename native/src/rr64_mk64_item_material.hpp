#pragma once
#ifdef __cplusplus
#include <array>
#include <cstdint>
#include <span>
namespace rr64::mk64_items {
struct MaterialCommand {
    std::uint32_t first = 0, second = 0;
    bool operator==(const MaterialCommand &) const = default;
};
// Original actor material uses TEXEL0 * ENVIRONMENT + PRIMITIVE, then
// passes the result through cycle two. Only this traced material is changed.
unsigned compile_material(std::span<const MaterialCommand> source,
                          std::span<MaterialCommand> output, unsigned rgba, bool ghost,
                          bool dark = false) noexcept;
void reset_material_session() noexcept;
}
extern "C" {
#endif
unsigned rr64_mk64_items_actor_list(unsigned char *, unsigned node, unsigned original);
// Returns the original list unchanged on refusal; success inserts two commands
// ahead of the native call, whose registers/cursor must then advance 16 bytes.
unsigned rr64_mk64_items_actor_call(unsigned char *, unsigned node, unsigned original,
                                    unsigned call);
#ifdef __cplusplus
}
#endif
