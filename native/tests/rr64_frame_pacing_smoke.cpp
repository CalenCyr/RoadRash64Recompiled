#include <cstdint>
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <string_view>

#include "hle/rt64_rr64_frame_pacing.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "RR64 frame-pacing smoke failure: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void verifyProductionMatchingGate() {
    // This source contract deliberately checks the real queue wiring as well
    // as the pure policy below. It rejects an unchanged R18 queue even when
    // that queue is compiled alongside the new helper.
    const auto queuePath = std::filesystem::path(__FILE__).parent_path()
        .parent_path() / "lib/rt64/src/hle/rt64_workload_queue.cpp";
    std::ifstream input(queuePath);
    require(input.is_open(), "the production workload queue must be available for its gate contract");
    std::string source((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    source = std::regex_replace(source,
        std::regex(R"(//[^\r\n]*|/\*[\s\S]*?\*/)"), "");
    source.erase(std::remove_if(source.begin(), source.end(),
        [](unsigned char c) { return std::isspace(c) != 0; }), source.end());
    const std::string_view expected =
        "constboolrequiresFrameMatching=RR64FramePacing::requiresFrameMatching("
        "stablePresentation,workloadConfig.raytracingEnabled,"
        "workloadConfig.targetRate,workload.viOriginalRate);";
    const auto decision = source.find(expected);
    require(decision != std::string::npos &&
        source.find(expected, decision + expected.size()) == std::string::npos,
        "the actual queue must use the tested scene-independent matching policy once");
    const auto guardedBlock = source.find("if(requiresFrameMatching){", decision);
    require(guardedBlock != std::string::npos,
        "the production matching call must have its policy guard");
    auto blockEnd = guardedBlock + std::string_view("if(requiresFrameMatching){").size();
    std::size_t depth = 1;
    while (blockEnd < source.size() && depth != 0) {
        if (source[blockEnd] == '{') { ++depth; }
        if (source[blockEnd] == '}') { --depth; }
        ++blockEnd;
    }
    const auto matchCall = source.find("curFrame.match(");
    require(depth == 0 && matchCall > guardedBlock && matchCall < blockEnd &&
        source.find("curFrame.match(", matchCall + 1) == std::string::npos,
        "the sole production frame match must stay inside the tested guard");
    require(source.find("constboolretainedRacePath=stablePresentation&&raceActive;") !=
        std::string::npos,
        "native matching policy must not broaden retained race image ownership");
}
}

int main(int argc, char** argv) {
    using namespace RT64::RR64FramePacing;

    verifyProductionMatchingGate();
    require(!requiresFrameMatching(true, false, 60, 60) &&
        !requiresFrameMatching(true, false, 60, 0) &&
        !requiresFrameMatching(true, false, 30, 60) &&
        !requiresFrameMatching(true, false, 0, 60),
        "stable native, unresolved, lower-rate, and disabled output skip correspondence in every scene");
    require(requiresFrameMatching(true, false, 60, 30) &&
        requiresFrameMatching(true, false, 120, 60) &&
        requiresFrameMatching(true, false, 144, 60) &&
        requiresFrameMatching(true, false, 60, 50),
        "known lower-rate sources retain both integral and non-integral interpolation matching");
    for (const std::uint32_t source : {0u, 15u, 30u, 60u, 120u}) {
        for (const std::uint32_t target : {0u, 30u, 60u, 120u, 144u}) {
            require(requiresFrameMatching(false, false, target, source) == (target > 0),
                "the disabled stable-presentation control keeps its original matching policy");
            require(requiresFrameMatching(true, true, target, source) &&
                requiresFrameMatching(false, true, target, source),
                "ray tracing keeps required velocity matching at every cadence");
        }
    }
    // Race -> unresolved results -> native results -> higher-refresh output
    // must not leave a stale scene-dependent matching policy behind.
    const std::uint32_t transitionSource[] = {60, 0, 60, 60, 0, 30, 60};
    const std::uint32_t transitionTarget[] = {60, 60, 60, 120, 120, 60, 60};
    const bool transitionMatching[] = {false, false, false, true, false, true, false};
    for (std::size_t i = 0; i < std::size(transitionSource); ++i) {
        require(requiresFrameMatching(true, false, transitionTarget[i],
            transitionSource[i]) == transitionMatching[i],
            "cadence transitions must enable matching only for the current interpolation requirement");
    }

    if (argc == 2) {
        const std::string_view expected(argv[1]);
        require(expected == "enabled" || expected == "disabled",
            "policy expectation must be enabled or disabled");
        require(stablePresentationEnabled() == (expected == "enabled"),
            "the process-start presentation switch must match the requested policy");
    }

    require(exactCadenceFrameCount(60, 30) == 2,
        "a known 30 Hz source has two output slots at 60 Hz");
    require(exactCadenceFrameCount(120, 30) == 4,
        "a known 30 Hz source has four output slots at 120 Hz");
    require(exactCadenceFrameCount(60, 15) == 4,
        "a known 15 Hz source has four output slots at 60 Hz");
    require(exactCadenceFrameCount(30, 30) == 1,
        "native-rate presentation must remain single-frame");
    require(exactCadenceFrameCount(60, 50) == 1,
        "non-integral fallback cadence must not over-present");
    require(exactCadenceFrameCount(60, 60) == 1 &&
        exactCadenceFrameCount(60, 0) == 1,
        "equal and unresolved source rates require one native output slot");

    require(presentationFrameCount(true, 2, 60, 30) == 2,
        "generated interpolation count must be preserved");
    require(presentationFrameCount(false, 1, 60, 30) == 2,
        "temporary interpolation loss must repeat at 60 Hz");
    require(!shouldAbandonRemainingGeneratedFrames(false),
        "a predicted deadline miss must not discard generated frames");
    require(shouldAbandonRemainingGeneratedFrames(true),
        "a genuinely newer workload may supersede stale interpolation");

    require(!usePresentWait(true, true, true, false, true),
        "D3D12 VSync must use Present(1) without a second wait");
    require(!usePresentWait(true, true, false, true, true),
        "D3D12 software pacing must not stack a DXGI wait");
    require(usePresentWait(true, false, true, false, true) &&
        !usePresentWait(false, false, true, false, true),
        "other backends retain their supported present-wait policy");
    require(usePresentWait(true, true, true, false, false),
        "the disabled candidate retains the maintenance control wait");

    StableDeadlinePacer deadlinePacer;
    DeadlineDecision deadline = deadlinePacer.schedule(1'000'000'000LL, 60);
    require(deadline.deadlineNanoseconds == 1'000'000'000LL && deadline.rebased,
        "the first presentation must establish an immediate deadline");
    deadline = deadlinePacer.schedule(1'005'000'000LL, 60);
    require(deadline.deadlineNanoseconds == 1'016'666'666LL && !deadline.rebased,
        "an early frame must retain the absolute 60 Hz deadline");
    deadline = deadlinePacer.schedule(1'040'000'000LL, 60);
    require(deadline.deadlineNanoseconds == 1'040'000'000LL && deadline.rebased,
        "a late frame must discard timing debt instead of bursting");
    deadline = deadlinePacer.schedule(1'041'000'000LL, 120);
    require(deadline.deadlineNanoseconds == 1'041'000'000LL && deadline.rebased,
        "a refresh-rate change must begin a new deadline stream");

    // Real timers can oversleep after schedule() returns. The old test injected
    // all lateness before schedule(), so it never exercised this burst path.
    deadlinePacer.reset();
    deadline = deadlinePacer.schedule(0, 60);
    deadlinePacer.recordPresent(0);
    require(deadline.deadlineNanoseconds == 0,
        "the first frame must establish the clock, not leave the next unpaced");
    deadline = deadlinePacer.schedule(1'000'000, 60);
    require(deadline.deadlineNanoseconds == 16'666'666,
        "the second cold-start frame must wait a complete interval");
    deadlinePacer.recordPresent(25'000'000);
    deadline = deadlinePacer.schedule(26'000'000, 60);
    require(deadline.deadlineNanoseconds == 41'666'666,
        "an overslept frame must not be followed by a catch-up burst");
    deadline = deadlinePacer.schedule(10'000'000'000LL, 60);
    deadlinePacer.recordPresent(10'000'000'000LL);
    require(deadline.rebased && deadline.deadlineNanoseconds == 10'000'000'000LL,
        "a pause or focus stall must discard all accumulated timing debt");
    deadline = deadlinePacer.schedule(10'001'000'000LL, 60);
    require(deadline.deadlineNanoseconds == 10'016'666'666LL,
        "the frame after a pause must resume with a full interval");
    deadlinePacer.schedule(10'002'000'000LL, 0);
    deadline = deadlinePacer.schedule(10'003'000'000LL, 60);
    require(deadline.rebased && deadline.deadlineNanoseconds == 10'003'000'000LL,
        "disabling the target clock must reset its next activation");

    // Exercise the host deadline stream for 24 hours at 60 Hz. Inject a
    // scheduler miss once a minute and prove that the following deadline is a
    // full interval after the late frame rather than a catch-up burst.
    deadlinePacer.reset();
    constexpr std::int64_t sixtyHzPeriodNanoseconds =
        1'000'000'000LL / 60LL;
    constexpr std::uint64_t presentCount24Hours =
        24ULL * 60ULL * 60ULL * 60ULL;
    std::int64_t hostNowNanoseconds = 10'000'000'000LL;
    std::int64_t previousDeadlineNanoseconds = hostNowNanoseconds;
    for (std::uint64_t presentIndex = 0;
         presentIndex < presentCount24Hours;
         presentIndex++)
    {
        const bool injectMiss =
            presentIndex > 0 && ((presentIndex % (60ULL * 60ULL)) == 0);
        if (injectMiss) {
            hostNowNanoseconds = previousDeadlineNanoseconds +
                (sixtyHzPeriodNanoseconds * 2);
        }
        else {
            hostNowNanoseconds = previousDeadlineNanoseconds;
        }

        deadline = deadlinePacer.schedule(hostNowNanoseconds, 60);
        if (injectMiss) {
            require(deadline.rebased &&
                deadline.deadlineNanoseconds == hostNowNanoseconds,
                "a long-session scheduler miss must discard timing debt");
        }
        else if (presentIndex > 0) {
            require(!deadline.rebased &&
                deadline.deadlineNanoseconds - previousDeadlineNanoseconds ==
                    sixtyHzPeriodNanoseconds,
                "an on-time deadline stream must remain exactly periodic");
        }

        previousDeadlineNanoseconds = deadline.deadlineNanoseconds;
        // A different miss occurs after the timer was scheduled, once every
        // minute between the pre-schedule misses. Neither may carry debt.
        const bool oversleep = (presentIndex % 3600u) == 1800u;
        if (oversleep) {
            previousDeadlineNanoseconds += 7'000'000LL;
        }
        deadlinePacer.recordPresent(previousDeadlineNanoseconds);
    }

    // Generic synthetic identities exercise bounded history. Three fixtures do
    // not assert that the game uses three rotating presentation buffers.
    const PresentationTargetIdentity targetA{ 0x00100000u, 320, 2 };
    const PresentationTargetIdentity targetB{ 0x00125800u, 320, 2 };
    const PresentationTargetIdentity targetC{ 0x0014B000u, 320, 2 };
    PresentationTargetHistory history;
    require(!history.contains(targetA),
        "an unpresented framebuffer must not be authorized");
    history.record(targetA);
    history.record(targetB);
    history.record(targetC);
    require(history.contains(targetA) && history.contains(targetB) &&
        history.contains(targetC),
        "all three synthetic targets must survive in presentation history");
    require(!history.contains({targetA.address, 640, targetA.siz}) &&
        !history.contains({targetA.address, targetA.width, 3}),
        "matching addresses with different width or pixel size are not identities");
    history.record({0, 320, 2});
    history.record({targetA.address, 0, 2});
    require(history.size() == 3,
        "invalid framebuffer identities must never enter presentation history");
    history.record(targetA);
    require(history.size() == 3,
        "re-presenting a framebuffer must refresh rather than duplicate it");
    require(allowPresentationHistoryTarget(true, 60, 30, history.contains(targetB)),
        "a recorded target remains eligible for a known lower-rate source");
    require(allowPresentationHistoryTarget(true, 60, 60, history.contains(targetB)),
        "a recorded equal-rate target can retain its native image");
    require(allowPresentationHistoryTarget(true, 60, 0, history.contains(targetB)),
        "a recorded target can retain a native image during timing warm-up");
    require(allowPresentationHistoryTarget(true, 30, 60, history.contains(targetB)),
        "target admission also permits native images above the output rate");
    require(!allowPresentationHistoryTarget(false, 60, 30, history.contains(targetB)),
        "menu framebuffers must not use the race presentation history");
    require(!allowPresentationHistoryTarget(true, 0, 30, history.contains(targetB)),
        "disabled output must not authorize a history target");
    require(!allowPresentationHistoryTarget(true, 60, 30, false) &&
        !allowPresentationHistoryTarget(true, 60, 60, false) &&
        !allowPresentationHistoryTarget(true, 60, 0, false),
        "unrecorded targets remain unauthorized at every source rate");
    history.record({targetA.address, 640, 3});
    history.eraseAddress(targetA.address);
    require(!history.contains(targetA) &&
        !history.contains({targetA.address, 640, 3}) &&
        history.contains(targetB) && history.contains(targetC),
        "discarding an allocation expires every identity at that address only");

    PresentationTargetHistory boundedHistory;
    for (std::uint32_t index = 0;
         index < PresentationTargetHistory::Capacity + 1;
         index++)
    {
        boundedHistory.record({ 0x00200000u + (index * 0x1000u), 320, 2 });
    }
    require(boundedHistory.size() == PresentationTargetHistory::Capacity,
        "presentation history must remain bounded");
    require(!boundedHistory.contains({ 0x00200000u, 320, 2 }),
        "the oldest identity must be evicted at capacity");

    // Simulate a known 30 Hz input for 24 hours, with three synthetic target
    // identities and periodic scene resets. This checks history and output
    // count arithmetic; it does not measure the game's source rate or execute
    // the real guest, workload queue, rendering, or presentation backend.
    constexpr std::uint32_t simulatedHours = 24;
    constexpr std::uint32_t sourceUpdatesPerSecond = 30;
    constexpr std::uint32_t workloadCount =
        simulatedHours * 60u * 60u * sourceUpdatesPerSecond;
    constexpr std::uint32_t workloadsPerScene =
        30u * 60u * sourceUpdatesPerSecond;
    constexpr std::uint32_t backlogPeriod = 10'007u;

    std::uint64_t presentedFrames = 0;
    std::uint64_t interpolatedWorkloads = 0;
    std::uint64_t unavailableWarmupWorkloads = 0;
    std::uint64_t backlogEvents = 0;
    std::uint32_t consecutiveUnavailable = 0;
    std::uint32_t worstUnavailableRun = 0;
    PresentationTargetHistory rotatingHistory;
    const PresentationTargetIdentity rotatingTargets[] = {
        targetA,
        targetB,
        targetC
    };
    for (std::uint32_t workload = 0; workload < workloadCount; workload++) {
        if ((workload % workloadsPerScene) == 0) {
            rotatingHistory.clear();
        }

        const PresentationTargetIdentity target = rotatingTargets[workload % 3];
        const bool interpolate = allowPresentationHistoryTarget(
            true,
            60,
            sourceUpdatesPerSecond,
            rotatingHistory.contains(target));

        const bool newerWorkloadPending =
            (workload != 0u) && ((workload % backlogPeriod) == 0u);
        if (newerWorkloadPending) {
            require(shouldAbandonRemainingGeneratedFrames(true),
                "a real producer backlog must supersede only its stale batch");
            backlogEvents++;
        }
        else {
            require(!shouldAbandonRemainingGeneratedFrames(false),
                "an on-time workload must never inherit an earlier backlog");
        }

        // The synthetic source is known to be 30 Hz. Missing history changes
        // interpolation eligibility, not the two-slot count for this fixture.
        presentedFrames += presentationFrameCount(interpolate, 2, 60,
            sourceUpdatesPerSecond);
        interpolatedWorkloads += interpolate ? 1u : 0u;
        unavailableWarmupWorkloads += interpolate ? 0u : 1u;
        consecutiveUnavailable = interpolate ? 0u : consecutiveUnavailable + 1u;
        worstUnavailableRun = std::max(
            worstUnavailableRun,
            consecutiveUnavailable);
        rotatingHistory.record(target);
    }

    require(presentedFrames == std::uint64_t(workloadCount) * 2,
        "a synthetic known 30 Hz stream retains its two-slot output count");
    const std::uint64_t simulatedScenes = workloadCount / workloadsPerScene;
    require(unavailableWarmupWorkloads == simulatedScenes * 3u,
        "each scene reset requires one warm-up visit per synthetic target");
    require(interpolatedWorkloads + unavailableWarmupWorkloads == workloadCount,
        "every workload must remain accounted for across history transitions");
    require(worstUnavailableRun == 3u,
        "history warm-up lasts three visits for the three-target fixture");

    std::cout
        << "RR64 synthetic frame-pacing stress passed: "
        << simulatedHours << " simulated hours, "
        << workloadCount << " source updates, "
        << presentedFrames << " modeled 60 Hz output slots, "
        << interpolatedWorkloads << " interpolated workloads, "
        << backlogEvents << " transient backlogs, worst warm-up "
        << worstUnavailableRun << " workloads, "
        << presentCount24Hours << " deadline decisions.\n";
    return EXIT_SUCCESS;
}
