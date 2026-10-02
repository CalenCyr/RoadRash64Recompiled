// Actual RT64 frame matching and projection processing; no window or GPU.
#include "hle/rt64_rr64_camera_boundary.h"
#include "render/rt64_projection_processor.h"
#include "render/rt64_transform_processor.h"
#include "rr64_highlight_render_boundary.hpp"
#include "rr64_engine_layout.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {
using namespace RT64;
unsigned failures = 0, checks = 0;
void check(bool ok, const char *why) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", why); }
}
unsigned read(unsigned char *m, unsigned address) {
    unsigned result = 0; rr64::engine::read_u32(m, address, result); return result;
}
void put(unsigned char *m, unsigned address, unsigned value) {
    rr64::engine::write_u32(m, address, value);
}
unsigned packet(unsigned char *m, unsigned epoch, unsigned key, unsigned views = 1) {
    constexpr unsigned base = 0x80200000u, start = base + 0x140u;
    put(m, 0x800a1830u, epoch); put(m, 0x8009cba4u, 0);
    put(m, 0x800ac658u, base); put(m, 0x8009cb90u, base);
    put(m, 0x800bc9a0u, 0x4650u); put(m, 0x800ac650u, start);
    rr64_highlight_render_begin(m, key);
    for (unsigned i = 0; i < views; ++i) rr64_highlight_render_projection(m);
    const unsigned end = read(m, 0x800ac650u);
    if (end == start) {
        rr64_highlight_render_end(m);
        check(read(m, 0x800ac650u) == start, "ordinary pre-replay draw emits no extra commands");
        return 0;
    }
    const unsigned id = read(m, start + 12);
    check(end - start == views * 32u, "bounded four-command projection marker per native load");
    for (unsigned p = start; p < end; p += 32) {
        check(read(m, p) == 0xe0525464u && read(m, p + 4) == 0x10000064u &&
              read(m, p + 8) == 0x6400000cu && read(m, p + 12) == id &&
              read(m, p + 16) == 0xaaau && read(m, p + 24) == 0xe0525464u &&
              read(m, p + 28) == 0x20000000u,
              "every view/source carries the same submitted identity and simple interpolation policy");
    }
    rr64_highlight_render_end(m);
    check(read(m, 0x800ac650u) == end + 32 && read(m, end + 8) == 0x6400000cu &&
          read(m, end + 12) == G_EX_ID_AUTO && read(m, end + 16) == 2u,
          "draw exit restores AUTO without pushing or leaking projection stack entries");
    return id;
}
hlslpp::float4x4 translated(float x) {
    return hlslpp::float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, x,0,0,1);
}
bool same(const hlslpp::float4x4 &a, const hlslpp::float4x4 &b) {
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            if (std::fabs(a[i][j] - b[i][j]) > .00001f) return false;
    return true;
}
float determinant3(const hlslpp::float4x4 &a) {
    return a[0][0] * (a[1][1]*a[2][2] - a[1][2]*a[2][1]) -
           a[0][1] * (a[1][0]*a[2][2] - a[1][2]*a[2][0]) +
           a[0][2] * (a[1][0]*a[2][1] - a[1][1]*a[2][0]);
}
void seed(Workload &w, unsigned marker, const hlslpp::float4x4 &view, float root, unsigned views) {
    w.reset();
    auto &d = w.drawData;
    d.transformGroups.clear(); d.worldTransformGroups.clear();
    d.worldTransforms.clear(); d.worldTransformVertexIndices.clear();
    TransformGroup camera;
    camera.matrixId = marker ? marker : G_EX_ID_AUTO;
    camera.decompose = false;
    camera.positionInterpolation = camera.rotationInterpolation =
        camera.scaleInterpolation = camera.skewInterpolation =
        camera.perspectiveInterpolation = G_EX_COMPONENT_INTERPOLATE;
    d.transformGroups.push_back(camera);
    d.viewProjTransformGroups.assign(views + 1, 0);
    const auto identity = hlslpp::float4x4::identity();
    d.viewTransforms.assign(views + 1, view);
    d.projTransforms.assign(views + 1, identity);
    d.viewProjTransforms.assign(views + 1, view);
    d.viewportOrigins.assign(views + 1, G_EX_ORIGIN_NONE);
    d.rspViewports.resize(views + 1); d.viewportClipRatios.resize((views + 1) * 4);
    w.fbPairCount = 1;
    auto &pair = w.fbPairs[0]; pair.projectionCount = views; pair.projections.resize(views);
    for (unsigned i = 0; i < views; ++i) {
        auto &p = pair.projections[i]; p.reset();
        p.type = Projection::Type::Perspective; p.transformsIndex = i + 1;
        p.scissorRect = {0, 0, 1280, 960};
        TransformGroup world;
        world.matrixId = 0x52520000u + i; world.ordering = G_EX_ORDER_LINEAR;
        world.decompose = false;
        world.positionInterpolation = world.rotationInterpolation = G_EX_COMPONENT_INTERPOLATE;
        d.transformGroups.push_back(world);
        d.worldTransformGroups.push_back(i + 1);
        d.worldTransforms.push_back(translated(root + i));
        d.worldTransformVertexIndices.push_back(0);
        GameCall call{}; call.callDesc.minWorldMatrix = call.callDesc.maxWorldMatrix = i;
        call.callDesc.triangleCount = 1; p.addGameCall(call);
    }
    // Also exercise the non-retained path: the cut must not depend on a global
    // race flag or on the optional interpolation certificate being enabled.
    w.presentationScene.raceActive = false;
}
void frame(GameFrame &f, unsigned index, unsigned views) {
    f.workloads = {index}; f.frameMap.workloads.resize(WORKLOAD_QUEUE_SIZE);
    for (unsigned i = 0; i < views; ++i)
        f.perspectiveScenes.push_back(GameScene{{{index, 0, i}}});
}
void render_projection(WorkloadQueue &q, GameFrame &cur, const GameFrame &prev, float weight) {
    ProjectionProcessor processor;
    ProjectionProcessor::ProcessParams p;
    p.workloadQueue = &q; p.curFrame = &cur;
    // Exactly threadRenderFrame's matchedFrame gate, including the aspect
    // adjustment path that still processes an unmatched native endpoint.
    p.prevFrame = cur.matched ? &prev : nullptr;
    p.curFrameWeight = weight; p.prevFrameWeight = 0; p.aspectRatioScale = 1;
    processor.process(p);
}
void run_pair(unsigned old_id, unsigned new_id, unsigned views, bool expect_cut, bool sharp) {
    auto q = std::make_unique<WorkloadQueue>();
    const auto old_view = sharp ? hlslpp::float4x4(-1,0,0,0, 0,1,0,0, 0,0,-1,0, 0,-10,20,1)
                               : translated(-2);
    const auto new_view = sharp ? hlslpp::float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,-10,20,1)
                               : translated(-4);
    seed(q->workloads[0], old_id, old_view, 1000, views);
    seed(q->workloads[1], new_id, new_view, 0, views);
    GameFrame previous, current; frame(previous, 0, views); frame(current, 1, views);
    bool velocity = true, tiles = true, lookat = true;
    current.match(nullptr, *q, previous, nullptr, velocity, tiles, lookat);
    check(!velocity && !tiles && !lookat, "boundary/matching has no GPU upload or tile work");
    if (expect_cut) {
        check(!current.matched && !current.frameMap.workloads[1].mapped &&
              !current.rr64InterpolationCompatible && current.rr64GeometryRejectionReasons == 1,
              "camera cut rejects the whole submitted pair before world matching");
        check(current.frameMap.workloads[1].transforms.empty(), "sector-relative world transforms cannot interpolate across cut");
    } else {
        check(current.matched && current.frameMap.workloads[1].mapped,
              "same clip/ordinary frames retain actual matching");
        for (unsigned i = 0; i < views; ++i)
            check(current.frameMap.workloads[1].transforms[i].mapped &&
                  current.frameMap.workloads[1].viewProjections[i + 1].mapped,
                  "same phase keeps both camera and object interpolation");
    }
    for (float weight : {.25f, .5f, .75f, 1.f}) {
        render_projection(*q, current, previous, weight);
        for (unsigned i = 1; i <= views; ++i) {
            const auto &actual = q->workloads[1].drawData.modViewTransforms[i];
            if (expect_cut) {
                check(same(actual, new_view), "actual projection processor uses complete authored results camera at every output weight");
            } else if (!sharp) {
                check(std::fabs(actual[3][0] - (-2.f - 2.f * weight)) < .0001f,
                      "within-clip camera remains smooth at every output weight");
            } else if (weight == .5f) {
                check(std::fabs(determinant3(actual)) < .00001f,
                      "old unmarked cut reproduces a singular synthetic camera despite valid authored endpoint cameras");
            }
        }
    }
}
}
int main() {
    std::vector<unsigned char> memory(rr64::engine::kRdramSize);
    auto *m = memory.data();
    check(packet(m, 1, 0) == 0, "ordinary racing is untouched before highlights");
    const auto replay = packet(m, 2, 1);
    check(replay != 0 && packet(m, 3, 1, 4) == replay, "same clip keeps submitted marker for all viewports");
    const auto angle = packet(m, 4, 2);
    const auto results = packet(m, 5, 0, 4);
    check(angle != replay && results != angle && results != 0, "clip/camera changes and exit establish distinct boundaries");
    check(packet(m, 6, 0) == results, "results camera resumes ordinary smooth matching immediately");
    check(packet(m, 1, 0) == 0, "guest restart retires the old replay marker");
    const auto restarted = packet(m, 2, 1);
    check(restarted != replay, "same-mapping restart never reuses preceding replay identity");
    put(m, 0x800ac650u, 0x807ffff8u);
    rr64_highlight_render_begin(m, 2); rr64_highlight_render_projection(m); rr64_highlight_render_end(m);
    check(read(m, 0x800ac650u) == 0x807ffff8u, "invalid native command capacity is left untouched");
    for (unsigned views = 1; views <= 4; ++views) {
        run_pair(0, 0, views, false, true); // Concrete old camera interpolation failure.
        run_pair(replay, results, views, true, true);
        run_pair(0, replay, views, true, true);
        run_pair(results, 0, views, true, true);
        run_pair(replay, angle, views, true, true);
        run_pair(replay, replay, views, false, false);
        run_pair(results, results, views, false, false);
    }
    std::printf("Highlight camera boundary: %s (%u checks, %u failures); actual RT64 matcher/projection, all views, no assets or window\n",
                failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
