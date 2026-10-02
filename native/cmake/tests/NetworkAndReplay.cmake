# Kept out of the normal playable build. This two-process target validates the
# direct host/client lobby, slot assignment, ready state, phase changes, and UDP
# packet compatibility without requiring a ROM or renderer.
set(RR64_NETPLAY_TEST_SOURCES
    src/rr64_netplay.cpp
    src/rr64_highlight_network.cpp
    src/rr64_highlight_recording.cpp
)
add_executable(RR64NetplaySmoke EXCLUDE_FROM_ALL
    tests/rr64_netplay_smoke.cpp ${RR64_NETPLAY_TEST_SOURCES})
target_link_libraries(RR64NetplaySmoke PRIVATE libzstd_static)
add_executable(RR64HighlightPoseSmoke EXCLUDE_FROM_ALL
    tests/rr64_highlight_pose_smoke.cpp src/rr64_highlight_pose.cpp
    src/rr64_actor_pose.cpp src/rr64_highlight_recording.cpp src/rr64_highlight_network.cpp)
target_link_libraries(RR64HighlightPoseSmoke PRIVATE libzstd_static)
target_include_directories(RR64HighlightPoseSmoke PRIVATE $<TARGET_PROPERTY:RR64NetplaySmoke,INCLUDE_DIRECTORIES>)
target_compile_features(RR64HighlightPoseSmoke PRIVATE cxx_std_20)
add_executable(RR64HighlightRecordingSmoke EXCLUDE_FROM_ALL
    tests/rr64_highlight_recording_smoke.cpp src/rr64_highlight_recording.cpp)
target_include_directories(RR64HighlightRecordingSmoke PRIVATE
    $<TARGET_PROPERTY:RR64NetplaySmoke,INCLUDE_DIRECTORIES>)
target_compile_features(RR64HighlightRecordingSmoke PRIVATE cxx_std_20)
include(tests/rr64_highlight_transition_fixture.cmake)
include(tests/rr64_highlight_camera_readiness_fixture.cmake)
include(tests/rr64_highlight_camera_terrain_fixture.cmake)
add_executable(RR64PostraceChannelSmoke EXCLUDE_FROM_ALL
    tests/rr64_authoritative_channel_smoke.cpp
    src/rr64_highlight_network.cpp src/rr64_highlight_recording.cpp)
add_executable(RR64Mk64ItemKernelSmoke EXCLUDE_FROM_ALL
    tests/rr64_mk64_item_kernel_smoke.cpp src/rr64_mk64_item_kernel.cpp)
target_include_directories(RR64Mk64ItemKernelSmoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(RR64Mk64ItemKernelSmoke PRIVATE cxx_std_20)
add_executable(RR64Mk64ItemReviewSmoke EXCLUDE_FROM_ALL
    tests/rr64_mk64_item_review_smoke.cpp src/rr64_mk64_item_kernel.cpp)
target_include_directories(RR64Mk64ItemReviewSmoke PRIVATE src)
target_compile_features(RR64Mk64ItemReviewSmoke PRIVATE cxx_std_20)
include(tests/rr64_mk64_item_input_fixture.cmake)
include(tests/rr64_mk64_item_presentation_fixture.cmake)
include(tests/rr64_mk64_item_hud_fixture.cmake)
add_executable(RR64Mk64ItemMaterialSmoke EXCLUDE_FROM_ALL
    tests/rr64_mk64_item_material_smoke.cpp src/rr64_mk64_item_material.cpp)
target_include_directories(RR64Mk64ItemMaterialSmoke PRIVATE
    $<TARGET_PROPERTY:RR64NetplaySmoke,INCLUDE_DIRECTORIES>)
target_compile_features(RR64Mk64ItemMaterialSmoke PRIVATE cxx_std_20)
add_executable(RR64Mk64ItemChannelSmoke EXCLUDE_FROM_ALL
    tests/rr64_mk64_item_channel_smoke.cpp
    src/rr64_highlight_network.cpp src/rr64_highlight_recording.cpp)
target_include_directories(RR64Mk64ItemChannelSmoke PRIVATE
    $<TARGET_PROPERTY:RR64NetplaySmoke,INCLUDE_DIRECTORIES>)
target_compile_features(RR64Mk64ItemChannelSmoke PRIVATE cxx_std_20)
target_link_libraries(RR64Mk64ItemChannelSmoke PRIVATE libzstd_static)
if(WIN32)
    target_link_libraries(RR64Mk64ItemChannelSmoke PRIVATE Ws2_32.lib)
    target_link_options(RR64Mk64ItemChannelSmoke PRIVATE "/STACK:8388608")
endif()
add_executable(RR64OnlineGuestFlowSmoke EXCLUDE_FROM_ALL tests/rr64_online_guest_flow_smoke.cpp)
add_executable(RR64OnlineRaceMemorySmoke EXCLUDE_FROM_ALL tests/rr64_online_race_memory_smoke.cpp)
add_executable(RR64PredictionPresentationSmoke EXCLUDE_FROM_ALL tests/rr64_prediction_presentation_smoke.cpp)
add_executable(RR64PredictionReconcileSmoke EXCLUDE_FROM_ALL tests/rr64_prediction_reconcile_smoke.cpp)
add_executable(RR64PredictionCameraSmoke EXCLUDE_FROM_ALL tests/rr64_prediction_camera_smoke.cpp)
add_executable(RR64PredictionCaptureSmoke EXCLUDE_FROM_ALL tests/rr64_prediction_capture_smoke.cpp)
add_executable(RR64PredictionCaseSmoke EXCLUDE_FROM_ALL tests/rr64_prediction_case_smoke.cpp)
add_executable(RR64AuthoritativeResamplingSmoke EXCLUDE_FROM_ALL tests/rr64_authoritative_resampling_smoke.cpp)
add_executable(RR64AuthoritativeRoundSmoke EXCLUDE_FROM_ALL tests/rr64_authoritative_round_smoke.cpp)
foreach(RR64_PREDICTION_CHECK RR64PredictionPresentationSmoke RR64PredictionReconcileSmoke RR64PredictionCaptureSmoke RR64PredictionCaseSmoke RR64PredictionCameraSmoke RR64AuthoritativeResamplingSmoke RR64AuthoritativeRoundSmoke)
    target_include_directories(${RR64_PREDICTION_CHECK} PRIVATE $<TARGET_PROPERTY:RR64NetplaySmoke,INCLUDE_DIRECTORIES>)
    target_compile_features(${RR64_PREDICTION_CHECK} PRIVATE cxx_std_20)
endforeach()
set(RR64_POSTRACE_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/rr64_online_postrace_fixture.cpp")
add_custom_command(OUTPUT "${RR64_POSTRACE_FIXTURE}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_online_postrace_fixture.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs/funcs_10.c" "${RR64_POSTRACE_FIXTURE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs/funcs_10.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs/funcs_7.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_online_postrace_fixture.py"
    VERBATIM)
target_sources(RR64OnlineGuestFlowSmoke PRIVATE "${RR64_POSTRACE_FIXTURE}")
target_compile_definitions(RR64OnlineGuestFlowSmoke PRIVATE RR64_POSTRACE_NATIVE_SETTERS=1)
foreach(RR64_POSTRACE_CHECK RR64PostraceChannelSmoke RR64OnlineGuestFlowSmoke RR64OnlineRaceMemorySmoke)
    target_include_directories(${RR64_POSTRACE_CHECK} PRIVATE $<TARGET_PROPERTY:RR64NetplaySmoke,INCLUDE_DIRECTORIES>)
    target_compile_features(${RR64_POSTRACE_CHECK} PRIVATE cxx_std_20)
endforeach()
target_link_libraries(RR64PostraceChannelSmoke PRIVATE libzstd_static)
if(WIN32)
    target_link_libraries(RR64PostraceChannelSmoke PRIVATE Ws2_32.lib)
    target_link_options(RR64PostraceChannelSmoke PRIVATE "/STACK:8388608")
endif()
target_include_directories(RR64NetplaySmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/rt64/src/contrib/zstd/lib"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/thirdparty/sse2neon"
)
if(WIN32)
    target_link_libraries(RR64NetplaySmoke PRIVATE Ws2_32.lib)
endif()
