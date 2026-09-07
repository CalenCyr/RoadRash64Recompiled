cmake_minimum_required(VERSION 3.20)
project(RoadRash64Recompiled LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT WIN32)
    message(FATAL_ERROR "The v0.3.0 native probe currently targets Windows x64 only.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Road Rash 64 Recompiled requires a 64-bit build.")
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/bin")
endforeach()

set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(N64MODERN_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/N64ModernRuntime")
set(RT64_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/rt64")
set(RECOMP_FRONTEND_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/RecompFrontend")
set(RECOMPILED_DIR "${ROOT_DIR}/build/RecompiledFuncs")

foreach(REQUIRED_PATH
    "${N64MODERN_RUNTIME_ROOT}/CMakeLists.txt"
    "${RT64_ROOT}/CMakeLists.txt"
    "${RECOMP_FRONTEND_ROOT}/CMakeLists.txt"
    "${RECOMPILED_DIR}/funcs.h"
    "${RECOMPILED_DIR}/funcs_0.c"
    "${RECOMPILED_DIR}/lookup.cpp"
    "${RECOMPILED_DIR}/recomp_overlays.inl"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/patch_helpers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/recompui_event_structs.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/ui_funcs.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rr64_runtime_shims.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/NotoEmoji-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/recomp.rcss"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/RoadRashLauncher.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/RoadRashLauncher-v3.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/achievement-toast-frame-v2.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/promptfont/promptfont.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/Caret.svg"
    "${RECOMP_FRONTEND_ROOT}/recompui/lib/RmlUi/Samples/assets/LatoLatin-Regular.ttf"
    "${RECOMP_FRONTEND_ROOT}/recompui/lib/RmlUi/Samples/assets/LICENSE.txt"
)
    if(NOT EXISTS "${REQUIRED_PATH}")
        message(FATAL_ERROR "Required native-stage input is missing: ${REQUIRED_PATH}")
    endif()
endforeach()

# Match the shared N64Recomp frontend stack used by current ports.
set(RT64_STATIC TRUE)
set(RT64_SDL_WINDOW_VULKAN TRUE)
set(BUILD_SHARED_LIBS OFF)
add_compile_definitions(HLSL_CPU NOMINMAX _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

add_subdirectory("${RT64_ROOT}" "${CMAKE_BINARY_DIR}/rt64")
add_subdirectory("${N64MODERN_RUNTIME_ROOT}" "${CMAKE_BINARY_DIR}/N64ModernRuntime")
target_include_directories(rt64 PRIVATE "${CMAKE_BINARY_DIR}/rt64/src")

add_library(RecompiledFuncs STATIC)
file(GLOB RR64_RECOMP_C CONFIGURE_DEPENDS "${RECOMPILED_DIR}/*.c")
file(GLOB RR64_RECOMP_CPP CONFIGURE_DEPENDS "${RECOMPILED_DIR}/*.cpp")
target_sources(RecompiledFuncs PRIVATE ${RR64_RECOMP_C} ${RR64_RECOMP_CPP})
target_include_directories(RecompiledFuncs PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
)
if(MSVC)
    target_compile_options(RecompiledFuncs PRIVATE
        /clang:-march=nehalem
        /clang:-fno-strict-aliasing
        /clang:-Wno-unused-variable
        /clang:-Wno-implicit-function-declaration
    )
else()
    target_compile_options(RecompiledFuncs PRIVATE
        -fno-strict-aliasing
        -Wno-unused-variable
        -Wno-implicit-function-declaration
    )
endif()

include(FetchContent)
set(SDL2_VERSION "2.30.3" CACHE STRING "SDL2 development package version")
FetchContent_Declare(
    sdl2
    URL "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-VC.zip"
)
FetchContent_MakeAvailable(sdl2)

# Voice chat uses Opus' low-latency speech mode. Build it statically with the
# port so release packages do not require another runtime DLL.
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    opus
    URL "https://downloads.xiph.org/releases/opus/opus-1.6.1.tar.gz"
    URL_HASH "SHA256=6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1"
)
FetchContent_MakeAvailable(opus)

# RecompFrontend's shader custom commands consume the DXC variables from the
# parent project. RT64 defines similar variables inside its own subdirectory,
# but those values do not propagate back to this scope.
if(MSVC)
    set(DXC_COMMON_OPTS "-I${CMAKE_CURRENT_SOURCE_DIR}/src")
    set(DXC_DXIL_OPTS "-Wno-ignored-attributes")
    set(DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
    set(DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T" "ps_6_0" "-D" "DYNAMIC_RENDER_PARAMS")
    set(DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T" "vs_6_0" "-D" "DYNAMIC_RENDER_PARAMS" "-fvk-invert-y")
    set(DXC_CS_OPTS "${DXC_COMMON_OPTS}" "-E" "CSMain" "-T" "cs_6_0")
    set(DXC_GS_OPTS "${DXC_COMMON_OPTS}" "-E" "GSMain" "-T" "gs_6_0")
    set(DXC_RT_OPTS "${DXC_COMMON_OPTS}" "-D" "RT_SHADER" "-T" "lib_6_3"
        "-fspv-target-env=vulkan1.1spirv1.4"
        "-fspv-extension=SPV_KHR_ray_tracing"
        "-fspv-extension=SPV_EXT_descriptor_indexing")
    set(DXC "${RT64_ROOT}/src/contrib/dxc/bin/x64/dxc.exe")
endif()

set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH "${N64MODERN_RUNTIME_ROOT}")
set(RECOMP_FRONTEND_RT64_PATH "${RT64_ROOT}")
add_subdirectory("${RECOMP_FRONTEND_ROOT}" "${CMAKE_BINARY_DIR}/RecompFrontend")
target_include_directories(recompui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(RoadRash64Recompiled
    src/main.cpp
    src/register_overlays.cpp
    src/rr64_achievement_audio.cpp
    src/rr64_music.cpp
    src/rr64_achievements.cpp
    src/rr64_actor_pose.cpp
    src/rr64_actor_presentation.cpp
    src/rr64_actor_render_snapshot.cpp
    src/rr64_actor_render_runtime.cpp
    src/rr64_actor_held_pose.cpp
    src/rr64_world_render.cpp
    src/rr64_world_camera.cpp
    src/rr64_video_mode.cpp
    src/rr64_world_terrain_assets.cpp
    src/rr64_world_terrain.cpp
    src/rr64_world_object_assets.cpp
    src/rr64_world_objects.cpp
    src/rr64_audio_output.cpp
    src/rr64_engine_contract.cpp
    src/rr64_engine_snapshot.cpp
    src/rr64_input_prompts.cpp
    src/rr64_menu_navigation.cpp
    src/rr64_name_entry.cpp
    src/rr64_terrain_residency.cpp
    src/rr64_terrain_snapshot.cpp
    src/rr64_netplay.cpp
    src/rr64_online_menu.cpp
    src/rr64_online_race_sync.cpp
    src/rr64_runtime_shims.cpp
    src/rr64_voice_chat.cpp
)

target_include_directories(RoadRash64Recompiled PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/thirdparty/sse2neon"
    "${RT64_ROOT}/src/contrib"
    "${RT64_ROOT}/src/contrib/hlslpp/include"
    "${RT64_ROOT}/src/contrib/dxc/inc"
    "${RT64_ROOT}/src"
    "${RT64_ROOT}/src/rhi"
    "${RT64_ROOT}/src/render"
    "${RT64_ROOT}/src/contrib/nativefiledialog-extended/src/include"
    "${RECOMP_FRONTEND_ROOT}/recompinput/include"
    "${RECOMP_FRONTEND_ROOT}/recompui/include"
    "${RECOMP_FRONTEND_ROOT}/recompui/src"
    "${sdl2_SOURCE_DIR}/include"
)

target_link_directories(RoadRash64Recompiled PRIVATE "${sdl2_SOURCE_DIR}/lib/x64")

if(MSVC)
    target_compile_options(RoadRash64Recompiled PRIVATE
        /clang:-march=nehalem
        /clang:-fno-strict-aliasing
        /clang:-fms-extensions
    )
    # Keep function identities distinct; useful for later patch/mod work.
    target_link_options(RoadRash64Recompiled PRIVATE /OPT:NOICF
        "$<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS>"
        "$<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>")
else()
    target_compile_options(RoadRash64Recompiled PRIVATE -march=nehalem -fno-strict-aliasing -fms-extensions)
endif()

target_link_libraries(RoadRash64Recompiled PRIVATE
    RecompiledFuncs
    recompui
    recompinput
    librecomp
    ultramodern
    rt64
    nfd
    SDL2
    opus
    Winmm.lib
    Ws2_32.lib
)

add_custom_command(TARGET RoadRash64Recompiled POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RoadRash64Recompiled>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
        "${RT64_ROOT}/src/contrib/dxc/bin/x64/dxil.dll"
        "${RT64_ROOT}/src/contrib/dxc/bin/x64/dxcompiler.dll"
        "$<TARGET_FILE_DIR:RoadRash64Recompiled>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:RoadRash64Recompiled>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RECOMP_FRONTEND_ROOT}/recompui/lib/RmlUi/Samples/assets/LatoLatin-Regular.ttf"
        "$<TARGET_FILE_DIR:RoadRash64Recompiled>/assets/LatoLatin-Regular.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RECOMP_FRONTEND_ROOT}/recompui/lib/RmlUi/Samples/assets/LICENSE.txt"
        "$<TARGET_FILE_DIR:RoadRash64Recompiled>/assets/RmlUi-Sample-Fonts-LICENSE.txt"
)

set_property(TARGET RoadRash64Recompiled PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${ROOT_DIR}")

# Kept out of the normal playable build. This two-process target validates the
# direct host/client lobby, slot assignment, ready state, phase changes, and UDP
# packet compatibility without requiring a ROM or renderer.
add_executable(RR64NetplaySmoke EXCLUDE_FROM_ALL
    tests/rr64_netplay_smoke.cpp
    src/rr64_netplay.cpp
)
target_include_directories(RR64NetplaySmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/thirdparty/sse2neon"
)
target_link_libraries(RR64NetplaySmoke PRIVATE Ws2_32.lib)

# ROM-free host-audio queue policy and telemetry validation.
add_executable(RR64AudioQueueSmoke EXCLUDE_FROM_ALL
    tests/rr64_audio_queue_smoke.cpp
    src/rr64_audio_output.cpp
)
target_include_directories(RR64AudioQueueSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
)

# ROM-free regression coverage for long-session 60/120 FPS presentation.
# In particular, this reproduces the former self-sustaining 60-to-30 FPS
# collapse after a transient late interpolation frame.
add_executable(RR64FramePacingSmoke EXCLUDE_FROM_ALL
    tests/rr64_frame_pacing_smoke.cpp
)
target_include_directories(RR64FramePacingSmoke PRIVATE
    "${RT64_ROOT}/src"
)

# Exercise the actual message-queue implementation with a small stub scheduler.
# This target never starts a game, renderer or host event thread.
set(RR64_SCHEDULER_EVENTS_SOURCE "${N64MODERN_RUNTIME_ROOT}/ultramodern/src/events.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RR64_SCHEDULER_EVENTS_SOURCE}")
file(READ "${RR64_SCHEDULER_EVENTS_SOURCE}" RR64_SCHEDULER_EVENTS_TEXT)
string(REGEX MATCHALL "void[ \t]+ultramodern::send_si_message[ \t]*\\([ \t]*\\)[ \t]*\\{"
    RR64_SI_SIGNATURES "${RR64_SCHEDULER_EVENTS_TEXT}")
list(LENGTH RR64_SI_SIGNATURES RR64_SI_SIGNATURE_COUNT)
string(REGEX MATCH "void[ \t]+ultramodern::send_si_message[ \t]*\\([ \t]*\\)[ \t]*\\{[^{}]*\\}"
    RR64_SI_PRODUCER "${RR64_SCHEDULER_EVENTS_TEXT}")
string(REGEX REPLACE "//[^\n]*" "" RR64_SI_PRODUCER_SHAPE "${RR64_SI_PRODUCER}")
if (NOT RR64_SI_SIGNATURE_COUNT EQUAL 1 OR NOT RR64_SI_PRODUCER_SHAPE MATCHES
    "^void[ \t]+ultramodern::send_si_message[ \t]*\\([ \t]*\\)[ \t]*\\{[ \t\r\n]*ultramodern::enqueue_external_message\\(events_context\\.si\\.mq,[ \t\r\n]*events_context\\.si\\.msg,[ \t\r\n]*false,[ \t\r\n]*(true|false)\\);[ \t\r\n]*\\}$")
    message(FATAL_ERROR "SI queue smoke extraction requires review: send_si_message shape changed")
endif()
set(RR64_SCHEDULER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/scheduler_queue")
file(MAKE_DIRECTORY "${RR64_SCHEDULER_GENERATED_DIR}")
file(WRITE "${RR64_SCHEDULER_GENERATED_DIR}/rr64_si_message_producer.inc" "${RR64_SI_PRODUCER}\n")
# Negative control changes only the retry flag in the extracted producer. It
# must fail the same regression; production source is never modified.
string(REGEX REPLACE ",[ \t\r\n]*(true|false)\\);" ", true);"
    RR64_SI_LEGACY_PRODUCER "${RR64_SI_PRODUCER}")
file(WRITE "${RR64_SCHEDULER_GENERATED_DIR}/rr64_si_message_producer_legacy.inc" "${RR64_SI_LEGACY_PRODUCER}\n")
foreach(RR64_QUEUE_TEST RR64SchedulerQueueSmoke RR64SchedulerQueueLegacySiSmoke)
    add_executable(${RR64_QUEUE_TEST} EXCLUDE_FROM_ALL tests/rr64_scheduler_queue_smoke.cpp)
    target_include_directories(${RR64_QUEUE_TEST} PRIVATE
        "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
        "${N64MODERN_RUNTIME_ROOT}/thirdparty"
        "${N64MODERN_RUNTIME_ROOT}/thirdparty/concurrentqueue"
        "${RR64_SCHEDULER_GENERATED_DIR}"
    )
    set_target_properties(${RR64_QUEUE_TEST} PROPERTIES CXX_STANDARD 20)
    if (WIN32)
        target_compile_definitions(${RR64_QUEUE_TEST} PRIVATE NOMINMAX)
    endif()
endforeach()
target_compile_definitions(RR64SchedulerQueueLegacySiSmoke PRIVATE
    RR64_SI_PRODUCER_INCLUDE="rr64_si_message_producer_legacy.inc")

add_executable(RR64FrameMetadataSmoke EXCLUDE_FROM_ALL
    tests/rr64_frame_metadata_smoke.cpp
)

add_executable(RR64AuthoredFrameSmoke EXCLUDE_FROM_ALL
    tests/rr64_authored_frame_smoke.cpp
)
add_executable(RR64FrameOverwriteSmoke EXCLUDE_FROM_ALL
    tests/rr64_frame_overwrite_smoke.cpp
)
add_executable(RR64GeometryMatchSmoke EXCLUDE_FROM_ALL
    tests/rr64_geometry_match_smoke.cpp
)
target_include_directories(RR64GeometryMatchSmoke PRIVATE
    "${RT64_ROOT}/src"
)
set_target_properties(RR64GeometryMatchSmoke PROPERTIES CXX_STANDARD 17)
add_executable(RR64WorldMatchingSmoke EXCLUDE_FROM_ALL tests/rr64_world_matching_smoke.cpp
    tests/rr64_matching_test_callbacks.cpp)
target_include_directories(RR64WorldMatchingSmoke PRIVATE "${RT64_ROOT}/src"
    "${RT64_ROOT}/src/contrib" "${RT64_ROOT}/src/contrib/hlslpp/include"
    "${RT64_ROOT}/src/contrib/dxc/inc")
target_link_libraries(RR64WorldMatchingSmoke PRIVATE rt64)
target_link_directories(RR64WorldMatchingSmoke PRIVATE "${sdl2_SOURCE_DIR}/lib/x64")
if(MSVC)
    target_compile_options(RR64WorldMatchingSmoke PRIVATE /clang:-march=nehalem)
endif()
add_executable(RR64AuthoredCadenceSmoke EXCLUDE_FROM_ALL
    tests/rr64_authored_cadence_smoke.cpp
)
foreach(RR64_CPU_TEST RR64OwnedTextureReuseSmoke RR64RSPVertexSmoke RR64WorldMatchingPerformanceSmoke RR64TMEMLoadSmoke RR64MatchingPreflightSmoke RR64RSPTriangleBatchSmoke RR64TranslationRejectionSmoke)
    if(RR64_CPU_TEST STREQUAL "RR64OwnedTextureReuseSmoke")
        set(RR64_CPU_FIXTURE tests/rr64_owned_texture_reuse_smoke.cpp)
    elseif(RR64_CPU_TEST STREQUAL "RR64RSPVertexSmoke")
        set(RR64_CPU_FIXTURE tests/rr64_rsp_vertex_smoke.cpp)
    elseif(RR64_CPU_TEST STREQUAL "RR64TMEMLoadSmoke")
        set(RR64_CPU_FIXTURE tests/rr64_tmem_load_smoke.cpp)
    elseif(RR64_CPU_TEST STREQUAL "RR64MatchingPreflightSmoke")
        set(RR64_CPU_FIXTURE tests/rr64_matching_preflight_smoke.cpp)
    elseif(RR64_CPU_TEST STREQUAL "RR64RSPTriangleBatchSmoke")
        set(RR64_CPU_FIXTURE tests/rr64_rsp_triangle_batch_smoke.cpp)
    elseif(RR64_CPU_TEST STREQUAL "RR64TranslationRejectionSmoke")
        set(RR64_CPU_FIXTURE tests/rr64_translation_rejection_smoke.cpp)
    else()
        set(RR64_CPU_FIXTURE tests/rr64_world_matching_performance_smoke.cpp)
    endif()
    add_executable(${RR64_CPU_TEST} EXCLUDE_FROM_ALL ${RR64_CPU_FIXTURE}
        tests/rr64_matching_test_callbacks.cpp)
    target_include_directories(${RR64_CPU_TEST} PRIVATE "${RT64_ROOT}" "${RT64_ROOT}/src"
        "${RT64_ROOT}/src/contrib" "${RT64_ROOT}/src/contrib/hlslpp/include"
        "${RT64_ROOT}/src/contrib/dxc/inc" "${sdl2_SOURCE_DIR}/include")
    target_link_libraries(${RR64_CPU_TEST} PRIVATE rt64)
    target_link_directories(${RR64_CPU_TEST} PRIVATE "${sdl2_SOURCE_DIR}/lib/x64")
    set_target_properties(${RR64_CPU_TEST} PROPERTIES CXX_STANDARD 17)
endforeach()
add_executable(RR64CoordinateConsistencySmoke EXCLUDE_FROM_ALL
    tests/rr64_coordinate_consistency_smoke.cpp
)
add_executable(RR64TMEMHashSmoke EXCLUDE_FROM_ALL tests/rr64_tmem_hash_smoke.cpp)
target_include_directories(RR64TMEMHashSmoke PRIVATE "${RT64_ROOT}/src" "${RT64_ROOT}/src/contrib")
target_compile_definitions(RR64TMEMHashSmoke PRIVATE NOMINMAX)
set_target_properties(RR64TMEMHashSmoke PROPERTIES CXX_STANDARD 17)
foreach(RR64_PRESENTATION_TEST RR64AuthoredCadenceSmoke RR64CoordinateConsistencySmoke)
    target_include_directories(${RR64_PRESENTATION_TEST} PRIVATE "${RT64_ROOT}/src")
    set_target_properties(${RR64_PRESENTATION_TEST} PROPERTIES CXX_STANDARD 17)
endforeach()
target_include_directories(RR64FrameOverwriteSmoke PRIVATE
    "${RT64_ROOT}/src"
)
target_include_directories(RR64AuthoredFrameSmoke PRIVATE
    "${RT64_ROOT}/src"
)
target_include_directories(RR64FrameMetadataSmoke PRIVATE
    "${RT64_ROOT}/src"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
)

# ROM-free renderer-owned Max LOD snapshot, allocation and rollback contract.
add_executable(RR64ActorRenderSnapshotSmoke EXCLUDE_FROM_ALL
    tests/rr64_actor_render_snapshot_smoke.cpp
    src/rr64_actor_render_snapshot.cpp
    src/rr64_actor_pose.cpp
)
target_include_directories(RR64ActorRenderSnapshotSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/thirdparty/sse2neon"
)

# Exercises the actual Max LOD adapter with synthetic RDRAM and bounded helper
# substitutes. This never calls original guest code or starts a renderer.
add_executable(RR64ActorRenderRuntimeSmoke EXCLUDE_FROM_ALL
    tests/rr64_actor_render_runtime_smoke.cpp
    src/rr64_actor_render_runtime.cpp
    src/rr64_actor_held_pose.cpp
    src/rr64_actor_render_snapshot.cpp
    src/rr64_actor_pose.cpp
)
target_include_directories(RR64ActorRenderRuntimeSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/thirdparty/sse2neon"
)

# Exercise the current adapter with original generated root, vector, quaternion,
# renderer and fixed-matrix helpers. The fixture bounds animation/shadow routing;
# it verifies coordinate and matrix parity without a ROM or a game launch.
find_program(RR64_TEST_POWERSHELL NAMES pwsh powershell)
if(RR64_TEST_POWERSHELL)
    set(RR64_ACTOR_MATH_GENERATED_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/actor_math/rr64_actor_math_functions.c")
    add_custom_command(
        OUTPUT "${RR64_ACTOR_MATH_GENERATED_SOURCE}"
        COMMAND "${RR64_TEST_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_actor_math_fixture.ps1"
            -OutputPath "${RR64_ACTOR_MATH_GENERATED_SOURCE}"
            -GeneratedDirectory "${RECOMPILED_DIR}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_actor_math_fixture.ps1"
            ${RR64_RECOMP_C} "${RECOMPILED_DIR}/funcs.h"
        COMMENT "Extracting original actor renderer and math helpers for headless regression"
        VERBATIM
    )
    add_executable(RR64ActorRenderMathSmoke EXCLUDE_FROM_ALL
        tests/rr64_actor_render_math_smoke.cpp
        tests/world_hooks_disabled.cpp
        src/rr64_actor_render_runtime.cpp
        src/rr64_actor_held_pose.cpp
        src/rr64_actor_render_snapshot.cpp
        src/rr64_actor_pose.cpp
        "${RR64_ACTOR_MATH_GENERATED_SOURCE}"
    )
    target_include_directories(RR64ActorRenderMathSmoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${RECOMPILED_DIR}"
        "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
        "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
        "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
        "${N64MODERN_RUNTIME_ROOT}/thirdparty/sse2neon"
    )
    if(MSVC)
        target_compile_options(RR64ActorRenderMathSmoke PRIVATE
            /clang:-march=nehalem /clang:-fno-strict-aliasing
            /clang:-Wno-unused-variable /clang:-Wno-implicit-function-declaration)
    else()
        target_compile_options(RR64ActorRenderMathSmoke PRIVATE
            -fno-strict-aliasing -Wno-unused-variable -Wno-implicit-function-declaration)
    endif()

    # Compare the private held-wheel evaluator to the original generated wheel
    # update at zero elapsed time. Extraction keeps the reference tied to the
    # configured game output instead of copying production arithmetic here.
    set(RR64_ACTOR_WHEEL_GENERATED_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/actor_wheel/rr64_actor_wheel_functions.c")
    add_custom_command(
        OUTPUT "${RR64_ACTOR_WHEEL_GENERATED_SOURCE}"
        COMMAND "${RR64_TEST_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_actor_wheel_fixture.ps1"
            -OutputPath "${RR64_ACTOR_WHEEL_GENERATED_SOURCE}"
            -GeneratedDirectory "${RECOMPILED_DIR}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_actor_wheel_fixture.ps1"
            ${RR64_RECOMP_C} "${RECOMPILED_DIR}/funcs.h"
        COMMENT "Extracting original wheel pose helpers for headless regression"
        VERBATIM
    )
    add_executable(RR64ActorHeldWheelSmoke EXCLUDE_FROM_ALL
        tests/rr64_actor_held_wheel_smoke.cpp
        src/rr64_actor_held_pose.cpp
        src/rr64_actor_render_snapshot.cpp
        src/rr64_actor_pose.cpp
        "${RR64_ACTOR_WHEEL_GENERATED_SOURCE}"
    )
    get_target_property(RR64_ACTOR_MATH_INCLUDES RR64ActorRenderMathSmoke INCLUDE_DIRECTORIES)
    get_target_property(RR64_ACTOR_MATH_OPTIONS RR64ActorRenderMathSmoke COMPILE_OPTIONS)
    target_include_directories(RR64ActorHeldWheelSmoke PRIVATE ${RR64_ACTOR_MATH_INCLUDES})
    target_compile_options(RR64ActorHeldWheelSmoke PRIVATE ${RR64_ACTOR_MATH_OPTIONS})

    # Actual held-pose evaluator and generated quaternion math, exercised
    # through the runtime's stage/publication path. Other visual helpers are
    # bounded substitutes; the wheel-only target covers original B948 parity.
    set(RR64_HELD_PAIR_GENERATED_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/actor_held_pair/rr64_actor_held_math_functions.c")
    add_custom_command(
        OUTPUT "${RR64_HELD_PAIR_GENERATED_SOURCE}"
        COMMAND "${RR64_TEST_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_actor_wheel_fixture.ps1"
            -OnlyMath
            -OutputPath "${RR64_HELD_PAIR_GENERATED_SOURCE}"
            -GeneratedDirectory "${RECOMPILED_DIR}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_actor_wheel_fixture.ps1"
            ${RR64_RECOMP_C} "${RECOMPILED_DIR}/funcs.h"
        COMMENT "Extracting original math for runtime held-pair integration"
        VERBATIM
    )
    add_executable(RR64ActorHeldPairSmoke EXCLUDE_FROM_ALL
        tests/rr64_actor_render_runtime_smoke.cpp
        src/rr64_actor_render_runtime.cpp
        src/rr64_actor_held_pose.cpp
        src/rr64_actor_render_snapshot.cpp
        src/rr64_actor_pose.cpp
        "${RR64_HELD_PAIR_GENERATED_SOURCE}"
    )
    target_include_directories(RR64ActorHeldPairSmoke PRIVATE ${RR64_ACTOR_MATH_INCLUDES})
    target_compile_options(RR64ActorHeldPairSmoke PRIVATE ${RR64_ACTOR_MATH_OPTIONS})
    target_compile_definitions(RR64ActorHeldPairSmoke PRIVATE RR64_TEST_REAL_HELD_MATH=1)

    set(RR64_FULL_WEIGHT_GENERATED_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/actor_full_weight/rr64_actor_full_weight_functions.c")
    add_custom_command(
        OUTPUT "${RR64_FULL_WEIGHT_GENERATED_SOURCE}"
        COMMAND "${RR64_TEST_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_actor_full_weight_fixture.ps1"
            -OutputPath "${RR64_FULL_WEIGHT_GENERATED_SOURCE}"
            -GeneratedDirectory "${RECOMPILED_DIR}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_actor_full_weight_fixture.ps1"
            ${RR64_RECOMP_C} "${RECOMPILED_DIR}/funcs.h"
        COMMENT "Extracting original full-weight animation helpers for headless regression"
        VERBATIM
    )
    add_executable(RR64ActorFullWeightSmoke EXCLUDE_FROM_ALL
        tests/rr64_actor_full_weight_smoke.cpp
        "${RR64_FULL_WEIGHT_GENERATED_SOURCE}"
    )
    target_include_directories(RR64ActorFullWeightSmoke PRIVATE ${RR64_ACTOR_MATH_INCLUDES})
    target_compile_options(RR64ActorFullWeightSmoke PRIVATE ${RR64_ACTOR_MATH_OPTIONS})

    set(RR64_CRASH_ANIMATION_GENERATED_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/actor_crash/rr64_actor_crash_functions.c")
    add_custom_command(
        OUTPUT "${RR64_CRASH_ANIMATION_GENERATED_SOURCE}"
        COMMAND "${RR64_TEST_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_actor_crash_fixture.ps1"
            -OutputPath "${RR64_CRASH_ANIMATION_GENERATED_SOURCE}"
            -GeneratedDirectory "${RECOMPILED_DIR}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_actor_crash_fixture.ps1"
            ${RR64_RECOMP_C} "${RECOMPILED_DIR}/funcs.h"
        COMMENT "Extracting original crash animation routing for headless regression"
        VERBATIM
    )
    add_executable(RR64ActorCrashAnimationSmoke EXCLUDE_FROM_ALL
        tests/rr64_actor_crash_animation_smoke.cpp
        "${RR64_CRASH_ANIMATION_GENERATED_SOURCE}"
    )
    target_include_directories(RR64ActorCrashAnimationSmoke PRIVATE ${RR64_ACTOR_MATH_INCLUDES})
    target_compile_options(RR64ActorCrashAnimationSmoke PRIVATE ${RR64_ACTOR_MATH_OPTIONS})

    function(rr64_generated_world_test target fixture test_source)
        set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated/${target}/original_functions.c")
        add_custom_command(
            OUTPUT "${generated}"
            COMMAND "${RR64_TEST_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/${fixture}"
                -OutputPath "${generated}" -GeneratedDirectory "${RECOMPILED_DIR}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${fixture}"
                ${RR64_RECOMP_C} "${RECOMPILED_DIR}/funcs.h"
            COMMENT "Extracting original helpers for ${target}"
            VERBATIM)
        add_executable(${target} EXCLUDE_FROM_ALL "tests/${test_source}" "${generated}" ${ARGN})
        target_include_directories(${target} PRIVATE ${RR64_ACTOR_MATH_INCLUDES})
        target_include_directories(${target} PRIVATE "${N64MODERN_RUNTIME_ROOT}/thirdparty/concurrentqueue"
            "${N64MODERN_RUNTIME_ROOT}/thirdparty")
        target_compile_options(${target} PRIVATE ${RR64_ACTOR_MATH_OPTIONS})
    endfunction()
    rr64_generated_world_test(RR64ActorFinishBlendSmoke generate_actor_finish_fixture.ps1
        rr64_actor_finish_blend_smoke.cpp src/rr64_actor_render_runtime.cpp
        src/rr64_actor_render_snapshot.cpp src/rr64_actor_pose.cpp src/rr64_actor_held_pose.cpp)
    rr64_generated_world_test(RR64VideoModeSmoke generate_video_mode_fixture.ps1
        rr64_video_mode_smoke.cpp src/rr64_actor_render_snapshot.cpp src/rr64_actor_pose.cpp)
    rr64_generated_world_test(RR64WorldTerrainAssetsSmoke generate_world_terrain_assets_fixture.ps1
        rr64_world_terrain_assets_smoke.cpp src/rr64_world_terrain_assets.cpp)
    rr64_generated_world_test(RR64WorldTerrainSmoke generate_world_terrain_assets_fixture.ps1
        rr64_world_terrain_smoke.cpp src/rr64_world_terrain_assets.cpp src/rr64_world_terrain.cpp
        src/rr64_actor_render_snapshot.cpp src/rr64_actor_pose.cpp)
    rr64_generated_world_test(RR64WorldObjectAssetsSmoke generate_world_object_assets_fixture.ps1
        rr64_world_object_assets_smoke.cpp src/rr64_world_object_assets.cpp)
    add_executable(RR64WorldAssetBatchSmoke EXCLUDE_FROM_ALL
        tests/rr64_world_asset_batch_smoke.cpp src/rr64_world_terrain_assets.cpp
        src/rr64_world_object_assets.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/generated/RR64WorldTerrainAssetsSmoke/original_functions.c"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/RR64WorldObjectAssetsSmoke/original_functions.c")
    target_include_directories(RR64WorldAssetBatchSmoke PRIVATE ${RR64_ACTOR_MATH_INCLUDES}
        "${N64MODERN_RUNTIME_ROOT}/thirdparty" "${N64MODERN_RUNTIME_ROOT}/thirdparty/concurrentqueue")
    target_compile_options(RR64WorldAssetBatchSmoke PRIVATE ${RR64_ACTOR_MATH_OPTIONS})
    add_executable(RR64WorldObjectsSmoke EXCLUDE_FROM_ALL tests/rr64_world_objects_smoke.cpp
        src/rr64_world_objects.cpp src/rr64_actor_render_snapshot.cpp src/rr64_actor_pose.cpp)
    target_include_directories(RR64WorldObjectsSmoke PRIVATE ${RR64_ACTOR_MATH_INCLUDES}
        "${N64MODERN_RUNTIME_ROOT}/thirdparty" "${N64MODERN_RUNTIME_ROOT}/thirdparty/concurrentqueue")
    target_compile_options(RR64WorldObjectsSmoke PRIVATE ${RR64_ACTOR_MATH_OPTIONS})
    rr64_generated_world_test(RR64WorldRenderSmoke generate_world_render_fixture.ps1
        rr64_world_render_smoke.cpp src/rr64_world_render.cpp
        src/rr64_world_camera.cpp src/rr64_actor_render_snapshot.cpp src/rr64_actor_pose.cpp)
else()
    message(STATUS "Skipping optional generated actor math tests: PowerShell was not found")
endif()

# ROM-free validation for the revision-specific guest-memory contract. This
# catches range arithmetic and live-mode classification regressions without
# launching the game.
add_executable(RR64EngineContractSmoke EXCLUDE_FROM_ALL
    tests/rr64_engine_contract_smoke.cpp
    src/rr64_actor_pose.cpp
    src/rr64_actor_presentation.cpp
    src/rr64_engine_snapshot.cpp
    src/rr64_terrain_residency.cpp
    src/rr64_terrain_snapshot.cpp
)
target_include_directories(RR64EngineContractSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/thirdparty/sse2neon"
)

# ROM-free validation for the Big Game name-entry grid navigation shim.
add_executable(RR64NameEntryNavigationSmoke EXCLUDE_FROM_ALL
    tests/rr64_name_entry_navigation_smoke.cpp
    src/rr64_name_entry.cpp
)
target_include_directories(RR64NameEntryNavigationSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/thirdparty/sse2neon"
)

# ROM-free validation for the shared stock-menu edge/repeat filter. Gameplay
# steering bypasses this filter and is covered by the runtime mode gate.
add_executable(RR64MenuNavigationSmoke EXCLUDE_FROM_ALL
    tests/rr64_menu_navigation_smoke.cpp
    src/rr64_menu_navigation.cpp
)
target_include_directories(RR64MenuNavigationSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ROM-free validation for exact Big Game promotion/completion achievement
# boundaries. The runtime hooks themselves are emitted by N64Recomp.
add_executable(RR64AchievementTriggerSmoke EXCLUDE_FROM_ALL
    tests/rr64_achievement_trigger_smoke.cpp
)
target_include_directories(RR64AchievementTriggerSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ROM-free procedural sting validation. This guarantees the achievement sound
# is audible, bounded, and self-terminating without opening an audio device.
add_executable(RR64AchievementAudioSmoke EXCLUDE_FROM_ALL
    tests/rr64_achievement_audio_smoke.cpp
    src/rr64_achievement_audio.cpp
)
target_include_directories(RR64AchievementAudioSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Test-only fourteen-peer capacity and canonical rider-snapshot validation.
# This does not create a renderer or require a ROM.
add_executable(RR64NetplayCapacity EXCLUDE_FROM_ALL
    tests/rr64_netplay_capacity.cpp
    src/rr64_netplay.cpp
)
target_include_directories(RR64NetplayCapacity PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/thirdparty/sse2neon"
)
target_link_libraries(RR64NetplayCapacity PRIVATE Ws2_32.lib)

# ROM-free validation for the proximity attenuation curve. Codec, microphone,
# and network transport are exercised by the normal build and netplay smoke.
add_executable(RR64VoiceChatSmoke EXCLUDE_FROM_ALL
    tests/rr64_voice_chat_smoke.cpp
)
target_include_directories(RR64VoiceChatSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ROM-free policy validation for peer-local online rendering. Online races use
# one full-screen camera per machine while original local multiplayer retains
# the stock split-screen layout.
add_executable(RR64OnlineViewportSmoke EXCLUDE_FROM_ALL
    tests/rr64_online_viewport_smoke.cpp
)
target_include_directories(RR64OnlineViewportSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(RR64OnlineViewportSmoke PRIVATE cxx_std_20)

add_executable(RR64MusicSmoke EXCLUDE_FROM_ALL tests/rr64_music_smoke.cpp)
add_executable(RR64ConnectionSmoke EXCLUDE_FROM_ALL tests/rr64_connection_smoke.cpp)
target_compile_features(RR64ConnectionSmoke PRIVATE cxx_std_20)
target_link_libraries(RR64ConnectionSmoke PRIVATE Ws2_32.lib)
target_include_directories(RR64MusicSmoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(RR64MusicSmoke PRIVATE SDL2)
target_include_directories(RR64MusicSmoke PRIVATE "${sdl2_SOURCE_DIR}/include")
target_include_directories(RR64MusicSmoke PRIVATE "${N64MODERN_RUNTIME_ROOT}/librecomp/include")
target_include_directories(RR64MusicSmoke PRIVATE "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include")
target_link_directories(RR64MusicSmoke PRIVATE "${sdl2_SOURCE_DIR}/lib/x64")
if(WIN32)
    target_link_libraries(RoadRash64Recompiled PRIVATE mfplat mfreadwrite mfuuid ole32)
    target_link_libraries(RR64MusicSmoke PRIVATE mfplat mfreadwrite mfuuid ole32)
endif()
add_executable(RR64NetplayLimitSmoke EXCLUDE_FROM_ALL tests/rr64_netplay_limit_smoke.cpp src/rr64_netplay.cpp)
target_include_directories(RR64NetplayLimitSmoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(RR64NetplayLimitSmoke PRIVATE Ws2_32.lib)

if(WIN32)
    enable_language(RC)
    target_sources(RoadRash64Recompiled PRIVATE assets/RoadRashIcon.rc)
    add_executable(RoadRash64DirectStart WIN32 src/rr64_direct_start.cpp assets/RoadRashIcon.rc)
    target_compile_features(RoadRash64DirectStart PRIVATE cxx_std_20)
    target_link_libraries(RoadRash64DirectStart PRIVATE User32.lib)
endif()
add_executable(RR64SettingsSmoke EXCLUDE_FROM_ALL tests/rr64_settings_smoke.cpp)
target_compile_features(RR64SettingsSmoke PRIVATE cxx_std_20)

add_executable(RR64RacerViewSmoke EXCLUDE_FROM_ALL tests/rr64_racer_view_smoke.cpp)
target_compile_features(RR64RacerViewSmoke PRIVATE cxx_std_20)

add_executable(RR64PopupInputSmoke EXCLUDE_FROM_ALL tests/rr64_popup_input_smoke.cpp)
target_compile_features(RR64PopupInputSmoke PRIVATE cxx_std_20)

add_executable(RR64ActionBindingsSmoke EXCLUDE_FROM_ALL tests/rr64_action_bindings_smoke.cpp)
target_compile_features(RR64ActionBindingsSmoke PRIVATE cxx_std_20)
target_link_libraries(RR64ActionBindingsSmoke PRIVATE recompinput)
target_include_directories(RR64ActionBindingsSmoke PRIVATE $<TARGET_PROPERTY:recompinput,INCLUDE_DIRECTORIES>)
