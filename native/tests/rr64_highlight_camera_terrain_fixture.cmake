# Native floor queries in private camera scratch; test assets are mathematical
# surfaces. A local ROM and retained stock snapshot are supplied only at run time.
set(_rr64_camera_terrain_dir "${CMAKE_CURRENT_BINARY_DIR}/highlight-camera-terrain-fixture")
add_custom_command(
    OUTPUT "${_rr64_camera_terrain_dir}/native.c" "${_rr64_camera_terrain_dir}/policy.cpp"
           "${_rr64_camera_terrain_dir}/rr64_highlight_camera_terrain_inputs.hpp"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_highlight_camera_terrain_fixture.py"
        --source "${CMAKE_CURRENT_SOURCE_DIR}/.." --output "${_rr64_camera_terrain_dir}"
    DEPENDS tests/generate_highlight_camera_terrain_fixture.py
        "${RECOMPILED_DIR}/funcs_2.c" "${RECOMPILED_DIR}/funcs_3.c"
        src/rr64_experimental_course_policy.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/mk64_importer/native_cell.py"
    VERBATIM)
add_executable(RR64HighlightCameraTerrainSmoke EXCLUDE_FROM_ALL
    tests/rr64_highlight_camera_terrain_smoke.cpp
    "${_rr64_camera_terrain_dir}/native.c" "${_rr64_camera_terrain_dir}/policy.cpp"
    "${_rr64_camera_terrain_dir}/rr64_highlight_camera_terrain_inputs.hpp"
    src/rr64_highlight_camera.cpp src/rr64_highlight_camera_terrain.cpp)
target_include_directories(RR64HighlightCameraTerrainSmoke PRIVATE
    "${_rr64_camera_terrain_dir}" "${RECOMPILED_DIR}"
    $<TARGET_PROPERTY:RR64NetplaySmoke,INCLUDE_DIRECTORIES>)
target_compile_features(RR64HighlightCameraTerrainSmoke PRIVATE cxx_std_20)
target_compile_definitions(RR64HighlightCameraTerrainSmoke PRIVATE RR64_EXPERIMENTAL_COURSE NOMINMAX)
