#include "rr64_highlight_pose.hpp"
#include "rr64_actor_pose.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace rr64::highlights {
namespace {
using namespace engine;
constexpr unsigned pose_bytes = 28;
struct Binding {
    ModelGraphTopologySnapshot graph{};
    std::array<std::uint32_t, maximum_bones> addresses{}, sources{};
    std::uint32_t entity = 0, type = 0;
    Pose pose{};
};
struct SavedPose {
    std::uint32_t address = 0;
    std::array<std::uint32_t, 7> words{};
};
struct Scope {
    unsigned char *memory = nullptr;
    unsigned count = 0;
    std::uint32_t node = 0, root_record = 0;
    bool normalized = false, source_selected = false;
    std::array<SavedPose, maximum_bones> saved{};
};
thread_local Scope scope;

void hash_identity_word(std::uint64_t &hash, unsigned value) noexcept {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        hash ^= static_cast<std::uint8_t>(value >> shift);
        hash *= 1099511628211ull;
    }
}

bool overlap(std::uint32_t a, unsigned as, std::uint32_t b, unsigned bs) noexcept {
    return std::uint64_t(a) < std::uint64_t(b) + bs && std::uint64_t(b) < std::uint64_t(a) + as;
}
bool inspect(unsigned char *m, std::uint32_t node, unsigned tier, Binding &out, bool pose_values = true,
             BindReport *report = nullptr) noexcept {
    const auto fail = [&](BindFailure reason) {
        if (report)
            report->failure = reason;
        return false;
    };
    if (!m || tier > 2 || (node & 3) || !valid_guest_range(node, actor_scene::node_minimum_size) ||
        !read_u32(m, node, out.type) || (out.type != 1 && out.type != 2) ||
        !read_u32(m, node + actor_scene::entity, out.entity) ||
        !valid_guest_range(out.entity, out.type == 1 ? bike::stride : rider::stride))
        return fail(BindFailure::Node);
    std::uint32_t first = 0;
    if (!read_u32(m, node + actor_scene::lod_models + tier * 4, first) ||
        !capture_model_graph_topology(m, first, out.graph) ||
        out.graph.records[0].type != actor_scene::model_record_triple_transform)
        return fail(BindFailure::Graph);
    auto &p = out.pose;
    p.valid = true;
    p.record_count = out.graph.record_count;
    p.lod = std::uint16_t(tier);
    // Native 1AD24 allocates a linked renderer record for each source node.
    // Its next_delta is heap spacing: another peer or an earlier local race
    // can allocate the same model differently. Bind by ordered source-template
    // identity and native push/pop flags, never those runtime link distances.
    // The strict engine graph hash remains unchanged for local cache checks.
    std::uint64_t portable = 14695981039346656037ull;
    hash_identity_word(portable, 0x33534f50); // POS3 identity contract.
    hash_identity_word(portable, p.record_count);
    unsigned root_source = 0;
    if (!read_u32(m, first + 0x14, root_source) || !valid_guest_range(root_source, 0x14))
        return fail(BindFailure::PosePointer);
    if (report) {
        report->graph = first;
        report->records = p.record_count;
        report->lod = p.lod;
    }
    for (unsigned i = 0; i < out.graph.record_count; ++i) {
        const auto &record = out.graph.records[i];
        unsigned source = 0;
        std::uint16_t flags = 0;
        if (!read_u32(m, record.address + 0x14, source) || !valid_guest_range(source, 4) ||
            !read_u16(m, record.address + 0xa, flags))
            return fail(BindFailure::PosePointer);
        hash_identity_word(portable, record.type);
        hash_identity_word(portable, flags & 0x7000); // Native root/push/pop, excluding visibility.
        hash_identity_word(portable, source - root_source);
        hash_identity_word(portable, record.first_transform);
        hash_identity_word(portable, record.transform_count);
        if (!record.transform_count)
            continue;
        if (!read_u32(m, record.address + 0xc, out.addresses[i]) || (out.addresses[i] & 3) ||
            !valid_guest_range(out.addresses[i], pose_bytes) ||
            !read_u32(m, record.address + 0x14, out.sources[i]) ||
            !valid_guest_range(out.sources[i], record.type == 0x13 ? 0x14 : 0x44))
            return fail(BindFailure::PosePointer);
        if (i == 0) {
            if (!read_u16(m, out.sources[i] + 0x12, p.source_bank) || p.source_bank > 2)
                return fail(BindFailure::SourceBank);
            if (report)
                report->bank = p.source_bank;
            continue;
        }
        auto &bone = p.bones[p.count++];
        bone.index = std::uint16_t(i);
        bone.type = record.type;
        if (pose_values) {
            for (unsigned j = 0; j < 7; ++j)
                if (!read_float(m, out.addresses[i] + j * 4, bone.values[j]))
                    return fail(BindFailure::PoseValues);
        } else {
            // Only describe the destination layout. Its dormant tier-zero
            // children may never have been animated in the live mapping.
            bone.values[6] = 1;
        }
    }
    p.topology = portable;
    if (report) {
        report->bones = p.count;
        report->topology = p.topology;
    }
    if (!valid_pose(p))
        return fail(BindFailure::PoseValues);
    // A corrupt pointer must not turn a visual override into physics, graph,
    // resource or another pose writes. Preflight the complete write set before
    // either capture admission or the first guest-memory mutation.
    std::uint32_t owner = 0;
    read_u32(m, out.entity + 4, owner);
    for (unsigned i = 0; i < out.graph.record_count; ++i) {
        const auto address = out.addresses[i];
        if (!address)
            continue;
        if (overlap(address, pose_bytes, node, actor_scene::node_minimum_size) ||
            overlap(address, pose_bytes, out.entity, out.type == 1 ? bike::stride : rider::stride) ||
            (valid_guest_range(owner, 0xe8) && overlap(address, pose_bytes, owner, 0xe8)))
            return fail(BindFailure::PoseOverlap);
        for (unsigned j = 0; j < out.graph.record_count; ++j) {
            if (overlap(address, pose_bytes, out.graph.records[j].address, actor_scene::model_record_minimum_size) ||
                (out.sources[j] && overlap(address, pose_bytes, out.sources[j],
                                          out.graph.records[j].type == 0x13 ? 0x14 : 0x44)) ||
                (i != j && out.addresses[j] && overlap(address, pose_bytes, out.addresses[j], pose_bytes)))
                return fail(BindFailure::PoseOverlap);
        }
    }
    return true;
}
bool finite_vector(const Vec3 &v) noexcept {
    for (float f : v)
        if (!std::isfinite(f) || std::abs(f) > 100000)
            return false;
    return true;
}
bool valid_rotation(const Quaternion &q) noexcept {
    double norm = 0;
    for (float f : q) {
        if (!std::isfinite(f) || std::abs(f) > 4)
            return false;
        norm += double(f) * f;
    }
    return norm > 1e-12 && norm <= 16;
}
bool detailed_bank_ready(unsigned char *m) noexcept {
    std::uint32_t view = 0, slot = 0, detailed_scale = 0, far_scale = 0;
    std::uint16_t valid = 0, normalization = 0;
    if (!read_u32(m, globals::active_viewport, view) || view >= 4 ||
        !read_u32(m, globals::actor_render_buffer_slot, slot) || slot >= 2 ||
        !read_u32(m, 0x8009DBB0, detailed_scale) || detailed_scale != 0x42c80000 ||
        !read_u32(m, 0x8009DBB4, far_scale) || far_scale != 0x41200000 ||
        !read_u16(m, 0x800B6B68 + view * 0x9c + 2 * 0x34 + 0x30, valid) || !valid ||
        !read_u16(m, 0x800B73E8 + view * 12 + 2 * 4 + slot * 2, normalization) || !normalization)
        return false;
    Matrix4x4Snapshot projection{}, camera{};
    const auto offset = view * 0x180 + 2 * 0x80 + slot * 0x40;
    return decode_n64_matrix(m, 0x800B6568 + offset, projection) &&
           decode_n64_matrix(m, 0x800B6DE8 + offset, camera) &&
           projection.values[0] != 0 && projection.values[5] != 0 && projection.values[11] != 0 &&
           camera.values[15] == 1 && camera.values[3] == 0 && camera.values[7] == 0 && camera.values[11] == 0;
}
} // namespace

bool capture_node(unsigned char *m, std::uint32_t node, unsigned tier, Pose &out) noexcept {
    out = {};
    // Do not enroll a currently overridden playback pose as new history.
    if (scope.memory)
        return false;
    Binding current{};
    if (!inspect(m, node, tier, current))
        return false;
    out = current.pose;
    return true;
}
bool capture_entity(unsigned char *m, std::uint32_t entity, unsigned type, Pose &out) noexcept {
    out = {};
    unsigned node = 0, actual_type = 0, owner = 0, current = 0, selected = 0;
    std::uint16_t tier = 0;
    if ((type != 1 && type != 2) ||
        !valid_guest_range(entity, type == 1 ? bike::stride : rider::stride) ||
        !read_u32(m, entity + 8, node) || (node & 3) ||
        !valid_guest_range(node, actor_scene::node_minimum_size) ||
        !read_u32(m, node, actual_type) || actual_type != type ||
        !read_u32(m, node + actor_scene::entity, owner) || owner != entity ||
        !read_u16(m, node + actor_scene::selected_lod, tier) || tier > 2 ||
        !read_u32(m, node + actor_scene::current_model, current) ||
        !read_u32(m, node + actor_scene::lod_models + tier * 4, selected) || current != selected)
        return false;
    return capture_node(m, node, tier, out);
}
bool capture_prepared_node(unsigned char *live, unsigned char *prepared,
                           std::uint32_t node, Pose &out) noexcept {
    out = {};
    if (!live || live == prepared || scope.memory)
        return false;
    Binding current{};
    std::uint32_t type = 0, entity = 0, first = 0, selected = 0;
    std::uint16_t tier = 0;
    ModelGraphTopologySnapshot graph{};
    if (!inspect(prepared, node, 0, current) ||
        !read_u16(prepared, node + actor_scene::selected_lod, tier) || tier != 0 ||
        !read_u32(prepared, node + actor_scene::current_model, selected) ||
        selected != current.graph.records[0].address ||
        !read_u32(live, node, type) || type != current.type ||
        !read_u32(live, node + actor_scene::entity, entity) || entity != current.entity ||
        !read_u32(live, entity + 8, selected) || selected != node ||
        !read_u32(live, node + actor_scene::lod_models, first) ||
        !capture_model_graph_topology(live, first, graph) ||
        !compatible_transform_topology(graph, current.graph))
        return false;
    for (unsigned i = 0; i < graph.record_count; ++i) {
        if (graph.records[i].address != current.graph.records[i].address)
            return false;
        if (!graph.records[i].transform_count)
            continue;
        std::uint32_t address = 0, source = 0;
        if (!read_u32(live, graph.records[i].address + 0xc, address) || address != current.addresses[i] ||
            !read_u32(live, graph.records[i].address + 0x14, source) || source != current.sources[i])
            return false;
        if (i == 0 && (!read_u16(live, source + 0x12, tier) || tier != current.pose.source_bank))
            return false;
    }
    out = current.pose;
    return true;
}
bool begin_actor(unsigned char *m, std::uint32_t node, unsigned tier, const Pose &recorded,
                 const Vec3 &anchor, const Quaternion &rotation, const Vec3 &camera,
                 BindReport *report) noexcept {
    if (report)
        *report = {};
    const auto fail = [&](BindFailure reason) {
        if (report)
            report->failure = reason;
        return false;
    };
    if (scope.memory)
        return fail(BindFailure::ActiveScope);
    if (!finite_vector(anchor) || !finite_vector(camera))
        return fail(BindFailure::WorldPosition);
    if (!valid_rotation(rotation))
        return fail(BindFailure::WorldRotation);
    Binding current{};
    if (!inspect(m, node, tier, current, false, report))
        return false;
    if (!compatible_pose(recorded, current.pose))
        return fail(BindFailure::Compatibility);
    float scale = 0;
    if (!read_float(m, 0x8009dbacu + recorded.source_bank * 4, scale) ||
        !std::isfinite(scale) || scale <= 0 || scale > 1000)
        return fail(BindFailure::Scale);
    std::array<float, 7> root{};
    const bool single_root = std::none_of(current.graph.records.begin() + 1,
                                         current.graph.records.begin() + current.graph.record_count,
                                         [](const auto &record) { return record.type == 0x13; });
    const bool normalized = recorded.source_bank == 1 && single_root && detailed_bank_ready(m);
    if (report) {
        report->scale = scale;
        report->normalized = normalized;
        for (unsigned i = 0; i < 3; ++i)
            report->root[i] = (anchor[i] - camera[i]) * scale;
    }
    for (unsigned i = 0; i < 3; ++i) {
        root[i] = (anchor[i] - camera[i]) * scale;
        if (!std::isfinite(root[i]) || std::abs(root[i] * (normalized ? 0.1f : 1.0f)) > 32760)
            return fail(BindFailure::RootRange); // Native matrices ultimately convert to 16.16.
    }
    std::copy(rotation.begin(), rotation.end(), root.begin() + 3);
    unsigned count = 0;
    for (unsigned i = 0; i < current.graph.record_count; ++i) {
        if (!current.addresses[i])
            continue;
        auto &saved = scope.saved[count++];
        saved.address = current.addresses[i];
        for (unsigned j = 0; j < 7; ++j)
            if (!read_u32(m, saved.address + j * 4, saved.words[j]))
                return fail(BindFailure::PoseRead);
    }
    // Everything that can fail has completed. These checked low-RDRAM spans
    // cannot become invalid during this single-thread-owned draw scope.
    scope.memory = m;
    scope.count = count;
    scope.node = node;
    scope.root_record = current.graph.records[0].address;
    scope.normalized = normalized;
    scope.source_selected = false;
    for (unsigned j = 0; j < 7; ++j)
        write_float(m, current.addresses[0] + j * 4, root[j]);
    for (unsigned i = 0; i < recorded.count; ++i) {
        const auto &bone = recorded.bones[i];
        const auto destination = current.addresses[bone.index];
        for (unsigned j = 0; j < 7; ++j)
            write_float(m, destination + j * 4, bone.values[j]);
    }
    return true;
}
unsigned root_source(unsigned char *m, unsigned node, unsigned record, unsigned original) noexcept {
    if (scope.memory != m || scope.node != node || scope.root_record != record || !scope.normalized || original != 1)
        return original;
    scope.source_selected = true;
    return 2;
}
bool scale_root_matrix(unsigned char *m, unsigned node, unsigned record, unsigned address) noexcept {
    if (scope.memory != m || scope.node != node || scope.root_record != record ||
        !scope.normalized || !scope.source_selected || (address & 3) || !valid_guest_range(address, 64))
        return false;
    std::array<float, 16> matrix{};
    for (unsigned i = 0; i < matrix.size(); ++i)
        if (!read_float(m, address + i * 4, matrix[i]) || !std::isfinite(matrix[i]))
            return false;
    // Same source1 -> source2 conversion as the certified live Max LOD path:
    // convert all XYZ columns, including translation, before native 16.16.
    // Child transforms, authored bone values and detailed vertices stay intact.
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned col = 0; col < 3; ++col) {
            auto &value = matrix[row * 4 + col];
            value *= 0.1f;
            if (!std::isfinite(value) || std::abs(value) >= 32768)
                return false;
        }
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned col = 0; col < 3; ++col)
            write_float(m, address + (row * 4 + col) * 4, matrix[row * 4 + col]);
    scope.source_selected = false;
    return true;
}
void end_actor() noexcept {
    if (!scope.memory)
        return;
    for (unsigned i = 0; i < scope.count; ++i)
        for (unsigned j = 0; j < 7; ++j)
            write_u32(scope.memory, scope.saved[i].address + j * 4, scope.saved[i].words[j]);
    scope.memory = nullptr;
    scope.count = 0;
    scope.normalized = scope.source_selected = false;
}
bool actor_bound() noexcept { return scope.memory != nullptr; }
bool detailed_projection_ready(unsigned char *m) noexcept { return detailed_bank_ready(m); }
const char *bind_failure_name(BindFailure reason) noexcept {
    constexpr std::array names{"none", "active-scope", "world-position", "world-rotation", "node",
        "graph", "pose-pointer", "source-bank", "pose-values", "pose-overlap", "compatibility",
        "scale", "root-range", "pose-read"};
    const auto index = static_cast<unsigned>(reason);
    return index < names.size() ? names[index] : "unknown";
}
} // namespace rr64::highlights
