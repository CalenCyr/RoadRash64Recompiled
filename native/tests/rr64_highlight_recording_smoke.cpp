#include "rr64_highlight_recording.hpp"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>

namespace {
std::atomic<unsigned long long> allocations{0};
unsigned long long checks = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL " << message << '\n';
        std::exit(1);
    }
}
bool close(float a, float b, double epsilon = 0.001) { return std::abs(double(a) - b) <= epsilon; }
} // namespace
void *operator new(std::size_t size) {
    ++allocations;
    if (auto *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void *p) noexcept { ::operator delete(p); }
void operator delete[](void *p, std::size_t) noexcept { ::operator delete(p); }

using namespace rr64::highlights;
namespace {
Pose pose(float offset = 0) {
    Pose p{};
    p.valid = true;
    p.count = 2;
    p.record_count = 4;
    p.topology = 0x12345678;
    p.bones[0] = {1, 0x12, {offset, 2, 3, 0, 0, 0, 1}};
    p.bones[1] = {3, 0x13, {4, 5, 6, 0, 0, 0, 1}};
    return p;
}
void frame(Frame &f, std::uint64_t tick, std::uint64_t time, float offset = 0) {
    f = {};
    f.tick = tick;
    f.time_us = time;
    for (unsigned i = 0; i < maximum_racers; ++i) {
        auto &r = f.racers[i];
        r.active = true;
        r.model = i;
        r.character = i + 1;
        r.weapon = i % 10;
        r.crash_flags = BikeAttached | RiderAttached;
        r.bike_origin = {offset + float(i * 3), 4, 5};
        r.rider_origin = {offset + float(i * 3), 4, 6};
        r.bike_anchor = {offset + float(i * 3), 4.1f, 5.5f};
        r.rider_anchor = {offset + float(i * 3), 4.2f, 6.5f};
        r.bike_pose = pose();
        r.rider_pose = pose(1);
    }
    auto &t = f.traffic[0];
    t.active = 1;
    t.kind = 1;
    t.model = 0xd8;
    t.id = 11;
    t.position = {offset, 10, 1};
    t.motion_valid = 1;
    t.motion[0] = offset;
    f.hazards.clock = unsigned(tick);
    f.hazards.count = 1;
    auto &h = f.hazards.poses[0];
    h.active = 1;
    h.position = {offset, 20, 3};
    h.model = 20;
    h.generation = 7;
}
void validity(Frame &a, Frame &b) {
    frame(a, 1, 0);
    check(valid_frame(a), "complete pointer-free frame valid");
    b = a;
    b.tick = 0;
    check(!valid_frame(b), "zero authored tick rejected");
    b = a;
    b.racers[13].bike_origin[2] = std::numeric_limits<float>::quiet_NaN();
    check(!valid_frame(b), "far canonical racer NaN rejected");
    b = a;
    b.racers[0].rider_rotation = {};
    check(!valid_frame(b), "zero world quaternion rejected");
    b = a;
    b.racers[0].bike_pose.count = 129;
    check(!valid_frame(b), "oversize local pose rejected");
    b = a;
    b.racers[0].bike_pose.bones[1].index = 1;
    check(!valid_frame(b), "duplicate bone ordinal rejected");
    b = a;
    b.racers[0].bike_pose.bones[1].index = 4;
    check(!valid_frame(b), "bone outside graph rejected");
    b = a;
    b.racers[0].bike_pose.bones[0].index = 0;
    check(!valid_frame(b), "camera-dependent root record rejected");
    b = a;
    b.racers[0].bike_pose.bones[0].type = 0x99;
    check(!valid_frame(b), "nontransform record rejected");
    b = a;
    b.racers[0].bike_pose.bones[0].values[6] = 0;
    check(!valid_frame(b), "zero child quaternion rejected");
    b = a;
    b.racers[0].bike_pose.valid = false;
    check(!valid_frame(b), "invalid pose cannot advertise bones");
    b = a;
    b.traffic[19].motion[63] = std::numeric_limits<float>::infinity();
    check(!valid_frame(b), "inactive traffic nonfinite data rejected");
    b = a;
    b.traffic[1] = b.traffic[0];
    check(!valid_frame(b), "duplicate traffic identity rejected");
    b = a;
    b.hazards.count = 129;
    check(!valid_frame(b), "oversize hazard roster rejected");
    b = a;
    b.hazards.poses[127].active = 1;
    check(!valid_frame(b), "hidden tail hazard payload rejected");
    b = a;
    auto &p = b.racers[0].bike_pose;
    p.count = 127;
    p.record_count = 128;
    for (unsigned i = 0; i < p.count; ++i)
        p.bones[i] = {std::uint16_t(i + 1), 0x12, {0, 0, 0, 0, 0, 0, 1}};
    check(valid_frame(b), "maximum 128 graph records including root accepted");
    check(sizeof(Frame) * (rolling_frames + maximum_clips * maximum_clip_frames) < memory_limit,
          "frame capacity below 128 MiB");
}
void interpolation(Frame &a, Frame &b, Frame &out, Frame &saved_a, Frame &saved_b) {
    for (float origin : {0.0f, -5000.0f, 5000.0f, -10000.0f, 10000.0f}) {
        frame(a, 10, 1000000, origin);
        frame(b, 11, 1100000, origin + 10);
        for (auto &r : b.racers) {
            r.bike_rotation = {0, 0, 0.70710678f, 0.70710678f};
            r.rider_rotation = {0, 0, 0, -1};
            r.bike_pose.bones[0].values[0] = 8;
            r.rider_pose.bones[0].values[0] = 9;
        }
        a.hazards.poses[0].rotation[1] = 65530;
        b.hazards.poses[0].rotation[1] = 6;
        saved_a = a;
        saved_b = b;
        for (unsigned slow_tick = 0; slow_tick <= 16; ++slow_tick) {
            const auto time = 1000000 + slow_tick * 6250;
            check(interpolate(a, b, time, out), "slow-motion sample succeeds");
            const double w = slow_tick / 16.0;
            for (unsigned i = 0; i < maximum_racers; ++i) {
                const auto &r = out.racers[i];
                check(close(r.bike_origin[0], origin + float(i * 3) + float(10 * w)), "all riders near/far bike smooth");
                check(close(r.rider_origin[0], origin + float(i * 3) + float(10 * w)), "separate rider body smooth");
                check(close(r.bike_anchor[2], 5.5f) && close(r.rider_anchor[2], 6.5f), "render anchors separate from origins");
                check(close(r.bike_pose.bones[0].values[0], float(8 * w)), "local child pose slow motion");
                check(close(std::abs(r.rider_rotation[3]), 1), "opposite quaternion sign remains unit");
                check(close(r.bike_rotation[2], float(std::sin(w * 0.7853981633974483)), 0.00001), "world rotation shortest-arc spherical");
            }
        }
        check(a == saved_a && b == saved_b, "sampling never mutates recorded physics or poses");
        check(interpolate(a, b, 1050000, out) && out.hazards.poses[0].rotation[1] == 0,
              "hazard angles interpolate across wrap");
        check(close(out.traffic[0].position[0], origin + 5), "recorded traffic position smooth");
        check(out.traffic[0].motion == a.traffic[0].motion, "native traffic matrices never linearly deformed");
        check(interpolate(a, b, 999999, out) && out == a, "first endpoint exact");
        check(interpolate(a, b, 1100000, out) && out == b, "last endpoint exact");
        check(interpolate(a, b, 1050000, b) && close(b.racers[0].bike_origin[0], origin + 5), "aliased output safe");
    }
    frame(a, 1, 1000000);
    frame(b, 2, 1100000, 10);
    for (unsigned reason = 0; reason < 7; ++reason) {
        saved_b = b;
        auto &r = saved_b.racers[0];
        switch (reason) {
        case 0: ++r.generation; break;
        case 1: r.crash_flags = Ejected; break;
        case 2: ++r.model; break;
        case 3: ++r.character; break;
        case 4: ++r.weapon; break;
        case 5: r.active = false; break;
        case 6: r.rider_origin[0] += 1000; break;
        }
        check(interpolate(a, saved_b, 1050000, out), "discontinuity sample accepted");
        check(out.racers[0] == a.racers[0], "discontinuity holds exact earlier racer");
        check(close(out.racers[1].bike_origin[0], 8), "one racer discontinuity does not freeze other racers");
        check(interpolate(a, saved_b, 1100000, out) && out.racers[0] == saved_b.racers[0], "discontinuity endpoint cuts exact");
    }
    for (unsigned reason = 0; reason < 5; ++reason) {
        saved_b = b;
        auto &p = saved_b.racers[0].bike_pose;
        switch (reason) {
        case 0: ++p.topology; break;
        case 1: ++p.lod; break;
        case 2: ++p.source_bank; break;
        case 3: p.bones[1].index = 2; break;
        case 4: p.bones[0].type = 0x13; break;
        }
        check(!compatible_pose(a.racers[0].bike_pose, p), "ordered topology mismatch rejected despite same counts");
        check(interpolate(a, saved_b, 1050000, out) && out.racers[0].bike_pose == a.racers[0].bike_pose,
              "incompatible bone topology held without blending");
    }
    saved_b = b;
    saved_b.time_us += 1000000;
    check(interpolate(a, saved_b, 1050000, out) && out.racers == a.racers, "missing frame gap hard holds");
    saved_b = b;
    saved_b.hazards.poses[0].generation++;
    check(interpolate(a, saved_b, 1050000, out) && out.hazards.poses[0] == a.hazards.poses[0], "hazard recovery holds");
    check(!interpolate(b, a, 1050000, out), "reversed frame interval rejected");
}
void recording(Recorder &r, Frame &f, Frame &out) {
    frame(f, 1, 0);
    check(!r.push(f), "recording requires begin");
    r.begin(7);
    const auto allocated = allocations.load();
    for (unsigned i = 0; i <= 660; ++i) {
        const auto time = std::uint64_t(i) * 1000000 / 30;
        frame(f, i + 1, time, float(i));
        if (i == 90) check(r.mark_crash(1, 5), "first crash mark");
        if (i == 96) check(r.mark_crash(2, 20), "stronger adjacent crash mark");
        if (i == 240) check(r.mark_crash(3, 10), "second distinct crash");
        if (i == 390) check(r.mark_crash(4, 30), "third distinct crash");
        if (i == 540) check(r.mark_crash(5, 40), "fourth strongest replaces weakest");
        check(r.push(f), "long race bounded push");
        if (i == 96) {
            check(r.update_pose(f.tick, 2, false, pose(7)), "first view bike pose merge");
            check(r.update_pose(f.tick, 2, true, pose(8)), "second view rider pose merge");
            check(r.update_pose(f.tick, 13, true, pose(9)), "distant fourth view canonical pose merge");
            check(!r.update_pose(f.tick - 1, 2, true, pose()), "stale view merge rejected");
        }
    }
    const auto clips = r.seal();
    check(allocations.load() == allocated, "push merge select and seal allocate no memory");
    check(clips.size() == 3, "three best clips retained through long race");
    check(clips[0].score == 40 && clips[1].score == 30 && clips[2].score == 20, "best-first deterministic score order");
    check(clips[2].event_tick == 97 && clips[2].slot == 2, "strongest nearby event chooses correct rider/time");
    check(r.statistics().deduplicated_events == 1 && r.statistics().discarded_events == 1, "dedup and replacement reported");
    for (const auto &clip : clips) {
        check(clip.frames.size() >= 151 && clip.frames.size() <= 152, "timestamp five-second clip at thirty fps");
        check(!clip.truncated_before && !clip.truncated_after && !clip.capacity_limited, "complete clip explicitly marked");
        check(clip.frames.front().time_us <= clip.event_time_us - before_us &&
                  clip.frames.back().time_us >= clip.event_time_us + after_us,
              "two seconds before and three after retained");
        for (std::size_t j = 1; j < clip.frames.size(); ++j)
            check(clip.frames[j].tick > clip.frames[j - 1].tick && clip.frames[j].time_us > clip.frames[j - 1].time_us,
                  "clip frame identities strictly ordered");
        check(sample(clip, clip.event_time_us, out), "clip event sample");
        if (clip.slot == 2) {
            check(out.racers[2].bike_pose.bones[0].values[0] == 7 && out.racers[2].rider_pose.bones[0].values[0] == 8,
                  "later split views merged into retained clip");
            check(out.racers[13].rider_pose.bones[0].values[0] == 9, "far split-screen pose retained");
        }
    }
    const Frame *stable = clips[0].frames.data();
    check(r.seal()[0].frames.data() == stable, "sealed clip references stable");
    check(!r.push(f) && !r.mark_crash(0, 1) && !r.update_pose(f.tick, 0, true, pose()), "sealed clips immutable");
    check(r.clips()[0].frames.data() == stable, "rejected operations leave sealed storage stable");
    r.begin();
    check(r.clips().empty() && !r.sealed() && r.statistics().accepted_frames == 0, "new session resets all public metadata");
    frame(f, 1, 0);
    check(r.mark_crash(0, 1) && r.push(f), "early crash accepts zero timestamp");
    frame(f, 2, 33333);
    check(r.push(f), "short clip second frame");
    check(!r.push(f), "duplicate authored frame rejected");
    const auto short_clips = r.seal();
    check(short_clips.size() == 1 && short_clips[0].truncated_before && short_clips[0].truncated_after,
          "early race end explicitly reports incomplete clip");
    r.begin();
    for (unsigned i = 0; i <= 600; ++i) {
        frame(f, i + 1, std::uint64_t(i) * 1000000 / 120, float(i));
        if (i == 300) check(r.mark_crash(0, 10), "high-rate crash mark");
        check(r.push(f), "high-rate bounded record");
    }
    const auto high_rate = r.seal();
    check(high_rate.size() == 1 && high_rate[0].frames.size() == maximum_clip_frames,
          "clip hard capacity never exceeded");
    check(high_rate[0].capacity_limited && high_rate[0].truncated_before && high_rate[0].truncated_after,
          "oversubscribed timestamp window explicitly reported");
    r.begin();
    check(!r.mark_crash(14, 1) && !r.mark_crash(0, 0) && !r.mark_crash(0, std::numeric_limits<float>::infinity()),
          "invalid event marks rejected");
    frame(f, 1, 0);
    f.racers[13].active = false;
    check(r.mark_crash(13, 100) && r.push(f) && r.seal().empty(), "inactive event cannot create clip");
    r.begin();
    check(r.mark_crash(0, 10) && r.seal().empty() && r.statistics().rejected_marks == 1,
          "unassigned pending crash explicitly rejected on seal");
    r.reset();
    check(!r.push(f) && r.clips().empty(), "reset disables recorder and clears views");
}
} // namespace
int main() {
    auto a = std::make_unique<Frame>(), b = std::make_unique<Frame>(), out = std::make_unique<Frame>();
    auto saved_a = std::make_unique<Frame>(), saved_b = std::make_unique<Frame>();
    Recorder recorder;
    check(recorder.memory_bytes() <= memory_limit, "full storage bounded below 128 MiB");
    validity(*a, *b);
    interpolation(*a, *b, *out, *saved_a, *saved_b);
    recording(recorder, *a, *out);
    std::cout << "{\"checks\":" << checks << ",\"frame_bytes\":" << sizeof(Frame)
              << ",\"recorder_bytes\":" << recorder.memory_bytes() << ",\"maximum_clips\":3,\"maximum_frames\":180,"
                 "\"guest_memory_access\":false,\"hot_path_allocations\":0}\n";
}
