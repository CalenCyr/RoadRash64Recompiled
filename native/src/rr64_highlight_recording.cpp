#include "rr64_highlight_recording.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace rr64::highlights {
namespace {
constexpr float maximum_coordinate = 100000.0f;
constexpr double maximum_blend_distance = 512.0;
constexpr std::uint64_t maximum_blend_gap_us = 250000;

template <std::size_t N> bool finite(const std::array<float, N> &v, float bound) noexcept {
    for (float f : v)
        if (!std::isfinite(f) || std::abs(f) > bound)
            return false;
    return true;
}
bool quaternion(const Quaternion &q) noexcept {
    if (!finite(q, 4.0f))
        return false;
    double length = 0;
    for (float f : q)
        length += double(f) * f;
    return length > 1.0e-12 && length <= 16.0;
}
bool nearby(const Vec3 &a, const Vec3 &b) noexcept {
    double distance = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const double delta = double(b[i]) - a[i];
        distance += delta * delta;
    }
    return distance <= maximum_blend_distance * maximum_blend_distance;
}
float blend(float a, float b, double weight) noexcept {
    return float(double(a) + (double(b) - a) * weight);
}
template <std::size_t N>
void blend_values(const std::array<float, N> &a, const std::array<float, N> &b, double weight,
                  std::array<float, N> &out) noexcept {
    for (unsigned i = 0; i < N; ++i)
        out[i] = blend(a[i], b[i], weight);
}
Quaternion blend_rotation(const Quaternion &a, const Quaternion &b, double weight) noexcept {
    // Normalize before shortest-arc spherical interpolation. Sign-equivalent
    // quaternions must not collapse at the midpoint of a slow-motion sample.
    std::array<double, 4> x{}, y{};
    double la = 0, lb = 0;
    for (unsigned i = 0; i < 4; ++i) {
        la += double(a[i]) * a[i];
        lb += double(b[i]) * b[i];
    }
    la = std::sqrt(la);
    lb = std::sqrt(lb);
    double dot = 0;
    for (unsigned i = 0; i < 4; ++i) {
        x[i] = a[i] / la;
        y[i] = b[i] / lb;
        dot += x[i] * y[i];
    }
    if (dot < 0) {
        dot = -dot;
        for (double &f : y)
            f = -f;
    }
    dot = std::clamp(dot, 0.0, 1.0);
    double wa = 1 - weight, wb = weight;
    if (dot < 0.9995) {
        const double angle = std::acos(dot), denominator = std::sin(angle);
        wa = std::sin((1 - weight) * angle) / denominator;
        wb = std::sin(weight * angle) / denominator;
    }
    Quaternion result{};
    double length = 0;
    for (unsigned i = 0; i < 4; ++i) {
        result[i] = float(x[i] * wa + y[i] * wb);
        length += double(result[i]) * result[i];
    }
    const double inverse = 1 / std::sqrt(length);
    for (float &f : result)
        f = float(f * inverse);
    return result;
}
void blend_pose(const Pose &a, const Pose &b, double weight, Pose &out) noexcept {
    if (!compatible_pose(a, b))
        return;
    for (unsigned i = 0; i < a.count; ++i) {
        for (unsigned axis = 0; axis < 3; ++axis)
            out.bones[i].values[axis] = blend(a.bones[i].values[axis], b.bones[i].values[axis], weight);
        Quaternion x{}, y{};
        std::copy_n(a.bones[i].values.begin() + 3, 4, x.begin());
        std::copy_n(b.bones[i].values.begin() + 3, 4, y.begin());
        const auto q = blend_rotation(x, y, weight);
        std::copy(q.begin(), q.end(), out.bones[i].values.begin() + 3);
    }
}
void blend_weapon(const WeaponPose &a, const WeaponPose &b, double weight, WeaponPose &out) noexcept {
    // Appearance/disappearance and weapon changes are authored cuts, but must
    // not interrupt the independently recorded rider/bike motion.
    if (!a.valid || !b.valid || a.model != b.model || a.count != b.count || a.topology != b.topology)
        return;
    for (unsigned i = 1; i < a.count; ++i) {
        for (unsigned axis = 0; axis < 3; ++axis)
            out.transforms[i][axis] = blend(a.transforms[i][axis], b.transforms[i][axis], weight);
        Quaternion x{}, y{};
        std::copy_n(a.transforms[i].begin() + 3, 4, x.begin());
        std::copy_n(b.transforms[i].begin() + 3, 4, y.begin());
        const auto q = blend_rotation(x, y, weight);
        std::copy(q.begin(), q.end(), out.transforms[i].begin() + 3);
    }
}
bool same_racer(const Racer &a, const Racer &b) noexcept {
    return a.active && b.active && a.model == b.model && a.character == b.character &&
           a.weapon == b.weapon && a.crash_flags == b.crash_flags && a.generation == b.generation &&
           nearby(a.bike_origin, b.bike_origin) && nearby(a.rider_origin, b.rider_origin) &&
           nearby(a.bike_anchor, b.bike_anchor) && nearby(a.rider_anchor, b.rider_anchor);
}
std::uint64_t difference(std::uint64_t a, std::uint64_t b) noexcept {
    return a > b ? a - b : b - a;
}
std::uint64_t end_time(std::uint64_t event_time) noexcept {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    return event_time > maximum - after_us ? maximum : event_time + after_us;
}
} // namespace

bool valid_pose(const Pose &pose) noexcept {
    if (!pose.valid)
        return pose.count == 0;
    if (pose.count > maximum_bones || !pose.record_count || pose.record_count > maximum_bones ||
        pose.count >= pose.record_count || pose.lod > 2 || pose.source_bank > 2 || !pose.topology)
        return false;
    unsigned previous = 0;
    for (unsigned i = 0; i < pose.count; ++i) {
        const auto &bone = pose.bones[i];
        if (bone.index <= previous || bone.index >= pose.record_count ||
            (bone.type != 0x12 && bone.type != 0x13) || !finite(bone.values, maximum_coordinate))
            return false;
        Quaternion q{};
        std::copy_n(bone.values.begin() + 3, 4, q.begin());
        if (!quaternion(q))
            return false;
        previous = bone.index;
    }
    return true;
}
bool valid_weapon_pose(const WeaponPose &p) noexcept {
    if (!p.valid)
        return p.count == 0;
    if (p.model >= 13 || !p.count || p.count > maximum_weapon_records || !p.topology)
        return false;
    for (unsigned i = 0; i < p.count; ++i) {
        if (!finite(p.transforms[i], maximum_coordinate))
            return false;
        Quaternion q{};
        std::copy_n(p.transforms[i].begin() + 3, 4, q.begin());
        if (!quaternion(q))
            return false;
    }
    return true;
}
bool valid_frame(const Frame &frame) noexcept {
    if (!frame.tick || !netplay::valid_course_hazard_state(frame.hazards) ||
        !mk64_items::valid(frame.items))
        return false;
    for (const auto &r : frame.racers) {
        // Asset identifiers are values, never graph or actor addresses. The
        // native bridge validates their model/character resource relationship.
        if (r.model > 0xffff || r.character > 0xffff || r.weapon > 0xffff || (r.crash_flags & ~15u) ||
            !finite(r.bike_origin, maximum_coordinate) || !finite(r.rider_origin, maximum_coordinate) ||
            !finite(r.bike_anchor, maximum_coordinate) || !finite(r.rider_anchor, maximum_coordinate) ||
            !quaternion(r.bike_rotation) || !quaternion(r.rider_rotation) ||
            !valid_pose(r.bike_pose) || !valid_pose(r.rider_pose) || !valid_weapon_pose(r.held_weapon))
            return false;
    }
    for (unsigned i = 0; i < frame.traffic.size(); ++i) {
        const auto &v = frame.traffic[i];
        if (!world_sync::valid(v) || !std::isfinite(v.road_distance) ||
            !finite(v.position, maximum_coordinate) || !finite(v.velocity, maximum_coordinate) ||
            !finite(v.angles, maximum_coordinate) || !finite(v.motion, maximum_coordinate) ||
            !finite(v.directions, maximum_coordinate))
            return false;
        if (v.active)
            for (unsigned j = 0; j < i; ++j)
                if (frame.traffic[j].active && frame.traffic[j].id == v.id)
                    return false;
    }
    return true;
}
bool compatible_pose(const Pose &a, const Pose &b) noexcept {
    if (!a.valid || !b.valid || !valid_pose(a) || !valid_pose(b) || a.count != b.count ||
        a.record_count != b.record_count || a.lod != b.lod || a.source_bank != b.source_bank ||
        a.topology != b.topology)
        return false;
    for (unsigned i = 0; i < a.count; ++i)
        if (a.bones[i].index != b.bones[i].index || a.bones[i].type != b.bones[i].type)
            return false;
    return true;
}
bool interpolate(const Frame &a, const Frame &b, std::uint64_t time, Frame &out) noexcept {
    if (!valid_frame(a) || !valid_frame(b) || b.time_us <= a.time_us || b.tick <= a.tick)
        return false;
    if (time <= a.time_us) {
        out = a;
        return true;
    }
    if (time >= b.time_us) {
        out = b;
        return true;
    }
    // A separate value also makes aliased output safe. No allocation, pointers
    // into recorded frames or mutation of their original contents is involved.
    Frame result = a;
    result.time_us = time;
    if (b.time_us - a.time_us <= maximum_blend_gap_us) {
        const double weight = double(time - a.time_us) / double(b.time_us - a.time_us);
        // Keep timer/inventory decisions on the preceding recorded sample.
        // Only a stable object's position can interpolate across samples;
        // creation, collision, release and expiry remain hard boundaries.
        if (a.items.enabled && b.items.enabled)
            for (unsigned i = 0; i < mk64_items::object_capacity; ++i) {
                const auto &x = a.items.objects[i], &y = b.items.objects[i];
                if (x.generation && x.generation == y.generation && x.owner == y.owner &&
                    x.kind == y.kind && x.mode == y.mode && x.bounces == y.bounces &&
                    nearby(x.position, y.position)) {
                    blend_values(x.position, y.position, weight, result.items.objects[i].position);
                    blend_values(x.velocity, y.velocity, weight, result.items.objects[i].velocity);
                }
            }
        for (unsigned i = 0; i < maximum_racers; ++i) {
            const auto &x = a.racers[i], &y = b.racers[i];
            auto &r = result.racers[i];
            if (!same_racer(x, y))
                continue;
            blend_values(x.bike_origin, y.bike_origin, weight, r.bike_origin);
            blend_values(x.rider_origin, y.rider_origin, weight, r.rider_origin);
            blend_values(x.bike_anchor, y.bike_anchor, weight, r.bike_anchor);
            blend_values(x.rider_anchor, y.rider_anchor, weight, r.rider_anchor);
            r.bike_rotation = blend_rotation(x.bike_rotation, y.bike_rotation, weight);
            r.rider_rotation = blend_rotation(x.rider_rotation, y.rider_rotation, weight);
            blend_pose(x.bike_pose, y.bike_pose, weight, r.bike_pose);
            blend_pose(x.rider_pose, y.rider_pose, weight, r.rider_pose);
            blend_weapon(x.held_weapon, y.held_weapon, weight, r.held_weapon);
        }
        for (unsigned i = 0; i < result.traffic.size(); ++i) {
            const auto &x = a.traffic[i], &y = b.traffic[i];
            auto &v = result.traffic[i];
            if (!x.active || !y.active || x.id != y.id || x.model != y.model || x.kind != y.kind ||
                x.motion_valid != y.motion_valid || !nearby(x.position, y.position))
                continue;
            blend_values(x.position, y.position, weight, v.position);
            blend_values(x.velocity, y.velocity, weight, v.velocity);
            // Full native traffic matrices/angles are recorded but never
            // linearly mixed into a non-rigid matrix. Playback rebuilds roots.
        }
        if (a.hazards.count == b.hazards.count && b.hazards.clock >= a.hazards.clock)
            for (unsigned i = 0; i < result.hazards.count; ++i) {
                const auto &x = a.hazards.poses[i], &y = b.hazards.poses[i];
                auto &p = result.hazards.poses[i];
                if (!x.active || x.active != y.active || x.model != y.model || x.generation != y.generation ||
                    !nearby(x.position, y.position))
                    continue;
                blend_values(x.position, y.position, weight, p.position);
                blend_values(x.velocity, y.velocity, weight, p.velocity);
                p.visual_scale = blend(x.visual_scale, y.visual_scale, weight);
                for (unsigned axis = 0; axis < 3; ++axis) {
                    int delta = (int(y.rotation[axis]) - int(x.rotation[axis]) + 32768) & 0xffff;
                    delta -= 32768;
                    p.rotation[axis] = std::uint16_t(int(x.rotation[axis]) + int(std::lround(delta * weight)));
                }
            }
    }
    out = result;
    return true;
}
bool sample(const Clip &clip, std::uint64_t time, Frame &out) noexcept {
    if (clip.frames.empty() || clip.frames.size() > maximum_clip_frames)
        return false;
    if (time <= clip.frames.front().time_us) {
        if (!valid_frame(clip.frames.front()))
            return false;
        out = clip.frames.front();
        return true;
    }
    if (time >= clip.frames.back().time_us) {
        if (!valid_frame(clip.frames.back()))
            return false;
        out = clip.frames.back();
        return true;
    }
    const auto after = std::upper_bound(clip.frames.begin(), clip.frames.end(), time,
                                        [](auto value, const Frame &f) { return value < f.time_us; });
    return after != clip.frames.begin() && after != clip.frames.end() &&
           interpolate(*(after - 1), *after, time, out);
}

struct Recorder::Storage {
    struct Candidate {
        std::array<Frame, maximum_clip_frames> frames{};
        unsigned count = 0, slot = 0;
        float score = 0;
        std::uint64_t event_tick = 0, event_time = 0;
        bool capacity_limited = false;
    };
    std::array<Frame, rolling_frames> rolling{};
    std::array<Candidate, maximum_clips> candidates{};
    std::array<Clip, maximum_clips> views{};
    std::array<float, maximum_racers> pending_scores{};
    std::array<bool, maximum_racers> pending{};
    Statistics statistics{};
    std::uint64_t session = 0;
    unsigned next = 0, count = 0, view_count = 0;
    bool sealed = false;

    const Frame &ordered(unsigned index) const noexcept {
        return rolling[(next + rolling_frames - count + index) % rolling_frames];
    }
    Frame &latest() noexcept { return rolling[(next + rolling_frames - 1) % rolling_frames]; }
    void start(Candidate &c, unsigned slot, float score) noexcept {
        const auto &frame = latest();
        c.count = 0;
        c.slot = slot;
        c.score = score;
        c.event_tick = frame.tick;
        c.event_time = frame.time_us;
        c.capacity_limited = false;
        const auto cutoff = frame.time_us > before_us ? frame.time_us - before_us : 0;
        unsigned first = 0;
        // Keep the preceding sample as well, so the requested time can be
        // sampled even when authored frame timestamps straddle the boundary.
        while (first + 1 < count && ordered(first + 1).time_us <= cutoff)
            ++first;
        for (unsigned i = first; i < count; ++i)
            c.frames[c.count++] = ordered(i);
    }
    void event(unsigned slot, float score) noexcept {
        const auto &frame = latest();
        if (!frame.racers[slot].active) {
            ++statistics.rejected_marks;
            return;
        }
        Candidate *duplicate = nullptr, *vacant = nullptr, *worst = nullptr;
        for (auto &c : candidates) {
            if (!c.count) {
                if (!vacant)
                    vacant = &c;
                continue;
            }
            // Retain the strongest crash per rider. Repeated spills by one
            // participant cannot occupy the entire bounded three-clip reel.
            if ((c.slot == slot || difference(c.event_time, frame.time_us) <= distinct_event_us) &&
                (!duplicate || c.score > duplicate->score))
                duplicate = &c;
            if (!worst || c.score < worst->score || (c.score == worst->score && c.event_time > worst->event_time))
                worst = &c;
        }
        if (duplicate) {
            ++statistics.deduplicated_events;
            if (score > duplicate->score) {
                start(*duplicate, slot, score);
                // A stronger event can bridge two previously distinct event
                // windows. Retire both neighbors, rather than publishing two
                // clips that became duplicates after replacing their focus.
                for (auto &c : candidates)
                    if (&c != duplicate && c.count &&
                        (c.slot == slot || difference(c.event_time, frame.time_us) <= distinct_event_us)) {
                        c.count = 0;
                        ++statistics.deduplicated_events;
                    }
            }
            return;
        }
        if (vacant)
            start(*vacant, slot, score);
        else if (worst && score > worst->score) {
            ++statistics.discarded_events;
            start(*worst, slot, score);
        } else
            ++statistics.discarded_events;
    }
};

Recorder::Recorder() : data_(std::make_unique<Storage>()) {
    static_assert(sizeof(Storage) <= memory_limit, "Highlight storage exceeds its hard memory limit");
}
Recorder::~Recorder() = default;
void Recorder::reset() noexcept {
    auto &s = *data_;
    s.session = 0;
    s.next = s.count = s.view_count = 0;
    s.sealed = false;
    s.pending.fill(false);
    s.pending_scores.fill(0);
    s.statistics = {};
    s.views = {};
    for (auto &c : s.candidates)
        c.count = 0;
}
void Recorder::begin(std::uint64_t session) noexcept {
    reset();
    data_->session = session;
}
bool Recorder::push(const Frame &frame) noexcept {
    auto &s = *data_;
    if (!s.session || s.sealed || !valid_frame(frame) ||
        (s.count && (frame.tick <= s.latest().tick || frame.time_us <= s.latest().time_us))) {
        ++s.statistics.rejected_frames;
        return false;
    }
    s.rolling[s.next] = frame;
    s.next = (s.next + 1) % rolling_frames;
    s.count = std::min(s.count + 1, rolling_frames);
    ++s.statistics.accepted_frames;
    for (auto &c : s.candidates) {
        if (!c.count || c.frames[c.count - 1].time_us >= end_time(c.event_time))
            continue;
        if (c.count < maximum_clip_frames)
            c.frames[c.count++] = frame;
        else
            c.capacity_limited = true;
    }
    // Deterministic highest score first when multiple riders crash together.
    for (unsigned n = 0; n < maximum_racers; ++n) {
        unsigned selected = maximum_racers;
        for (unsigned i = 0; i < maximum_racers; ++i)
            if (s.pending[i] && (selected == maximum_racers || s.pending_scores[i] > s.pending_scores[selected]))
                selected = i;
        if (selected == maximum_racers)
            break;
        s.event(selected, s.pending_scores[selected]);
        s.pending[selected] = false;
    }
    return true;
}
bool Recorder::update_pose(std::uint64_t tick, unsigned slot, bool rider, const Pose &pose) noexcept {
    auto &s = *data_;
    if (!s.session || s.sealed || !s.count || tick != s.latest().tick || slot >= maximum_racers ||
        !s.latest().racers[slot].active || !pose.valid || !valid_pose(pose)) {
        ++s.statistics.rejected_poses;
        return false;
    }
    const auto &previous = rider ? s.latest().racers[slot].rider_pose : s.latest().racers[slot].bike_pose;
    // A later camera may still draw the original coarse model. Its held pose
    // must not replace the same authored frame's complete detailed snapshot.
    if (previous.valid && previous.lod < pose.lod) {
        ++s.statistics.rejected_poses;
        return false;
    }
    auto set_pose = [&](Frame &f) {
        auto &r = f.racers[slot];
        (rider ? r.rider_pose : r.bike_pose) = pose;
    };
    set_pose(s.latest());
    for (auto &c : s.candidates)
        if (c.count && c.frames[c.count - 1].tick == tick)
            set_pose(c.frames[c.count - 1]);
    return true;
}
bool Recorder::update_weapon(std::uint64_t tick, unsigned slot, const WeaponPose &pose) noexcept {
    auto &s = *data_;
    if (!s.session || s.sealed || !s.count || tick != s.latest().tick || slot >= maximum_racers ||
        !s.latest().racers[slot].active || !valid_weapon_pose(pose))
        return false;
    s.latest().racers[slot].held_weapon = pose;
    for (auto &c : s.candidates)
        if (c.count && c.frames[c.count - 1].tick == tick)
            c.frames[c.count - 1].racers[slot].held_weapon = pose;
    return true;
}
bool Recorder::mark_crash(unsigned slot, float score) noexcept {
    auto &s = *data_;
    if (!s.session || s.sealed || slot >= maximum_racers || !std::isfinite(score) || score <= 0 || score > 1.0e9f) {
        ++s.statistics.rejected_marks;
        return false;
    }
    ++s.statistics.crash_marks;
    if (s.pending[slot])
        ++s.statistics.deduplicated_events;
    s.pending_scores[slot] = s.pending[slot] ? std::max(score, s.pending_scores[slot]) : score;
    s.pending[slot] = true;
    return true;
}
std::span<const Clip> Recorder::seal() noexcept {
    auto &s = *data_;
    if (s.sealed)
        return clips();
    s.sealed = true;
    for (bool pending : s.pending)
        if (pending)
            ++s.statistics.rejected_marks;
    s.pending.fill(false);
    s.view_count = 0;
    for (auto &c : s.candidates) {
        if (!c.count)
            continue;
        auto &view = s.views[s.view_count++];
        view.slot = c.slot;
        view.score = c.score;
        view.event_tick = c.event_tick;
        view.event_time_us = c.event_time;
        view.truncated_before = c.event_time - c.frames[0].time_us < before_us;
        view.truncated_after = c.frames[c.count - 1].time_us < end_time(c.event_time);
        view.capacity_limited = c.capacity_limited;
        view.frames = {c.frames.data(), c.count};
        if (view.truncated_before || view.truncated_after || view.capacity_limited)
            ++s.statistics.truncated_clips;
    }
    std::sort(s.views.begin(), s.views.begin() + s.view_count, [](const Clip &a, const Clip &b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.event_time_us != b.event_time_us ? a.event_time_us < b.event_time_us : a.slot < b.slot;
    });
    return clips();
}
std::span<const Clip> Recorder::clips() const noexcept {
    return {data_->views.data(), data_->view_count};
}
const Statistics &Recorder::statistics() const noexcept { return data_->statistics; }
bool Recorder::sealed() const noexcept { return data_->sealed; }
std::size_t Recorder::memory_bytes() const noexcept { return sizeof(Storage); }
} // namespace rr64::highlights
