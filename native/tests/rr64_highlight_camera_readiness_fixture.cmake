# Execute the original cell-state lookup and camera collapse branch with
# synthetic terrain records. The real handoff camera owner is linked below.
set(_rr64_camera_readiness_dir "${CMAKE_CURRENT_BINARY_DIR}/highlight-camera-readiness-fixture")
set(_rr64_camera_readiness_native "${_rr64_camera_readiness_dir}/rr64_highlight_camera_readiness_native.cpp")
set(_rr64_camera_readiness_generator "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_highlight_camera_readiness_fixture.py")
add_custom_command(OUTPUT "${_rr64_camera_readiness_native}"
    COMMAND "${Python3_EXECUTABLE}" "${_rr64_camera_readiness_generator}"
        --recompiled-dir "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs"
        --config "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
        --output "${_rr64_camera_readiness_native}"
    DEPENDS "${_rr64_camera_readiness_generator}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs/funcs_3.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs/funcs_14.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
    VERBATIM)
add_executable(RR64HighlightReturnCameraSmoke EXCLUDE_FROM_ALL
    tests/rr64_highlight_camera_readiness_smoke.cpp
    "${_rr64_camera_readiness_native}" src/rr64_highlight_camera.cpp
    tests/rr64_highlight_camera_terrain_unavailable.cpp)
target_include_directories(RR64HighlightReturnCameraSmoke PRIVATE
    $<TARGET_PROPERTY:RR64NetplaySmoke,INCLUDE_DIRECTORIES>)
target_compile_features(RR64HighlightReturnCameraSmoke PRIVATE cxx_std_20)
if(MSVC)
    target_compile_definitions(RR64HighlightReturnCameraSmoke PRIVATE NOMINMAX)
endif()
