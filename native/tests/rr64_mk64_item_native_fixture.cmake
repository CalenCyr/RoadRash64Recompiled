# Native crash/physics fixture consumes a private saved RDRAM file at runtime.
set(_rr64_item_generated "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs")
set(_rr64_item_fixture "${CMAKE_CURRENT_BINARY_DIR}/rr64_mk64_item_native_fixture.c")
set(_rr64_item_walls_fixture "${CMAKE_CURRENT_BINARY_DIR}/rr64_mk64_item_native_fixture_walls.cpp")
set(_rr64_item_hud_fixture "${CMAKE_CURRENT_BINARY_DIR}/rr64_mk64_item_native_hud_fixture.c")
set(_rr64_item_input_fixture "${CMAKE_CURRENT_BINARY_DIR}/rr64_mk64_item_native_input_fixture.inc")
file(GLOB _rr64_item_generated_sources "${_rr64_item_generated}/funcs_*.c")
add_custom_command(OUTPUT "${_rr64_item_fixture}" "${_rr64_item_walls_fixture}" "${_rr64_item_input_fixture}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_mk64_item_native_fixture.py"
        "${_rr64_item_generated}" "${_rr64_item_fixture}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_mk64_item_native_fixture.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rr64_course_walls.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml" ${_rr64_item_generated_sources}
    VERBATIM)
add_custom_command(OUTPUT "${_rr64_item_hud_fixture}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_mk64_item_hud_fixture.py"
        "${_rr64_item_generated}" "${_rr64_item_hud_fixture}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_mk64_item_hud_fixture.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml" ${_rr64_item_generated_sources}
    VERBATIM)
add_executable(RR64MK64ItemNativeSmoke EXCLUDE_FROM_ALL tests/rr64_mk64_item_native_smoke.cpp
    src/rr64_mk64_items.cpp src/rr64_mk64_item_kernel.cpp src/rr64_course_items.cpp
    src/rr64_mk64_item_hud.cpp src/rr64_course_impact.cpp
    "${_rr64_item_fixture}" "${_rr64_item_walls_fixture}" "${_rr64_item_hud_fixture}" "${_rr64_item_input_fixture}")
target_compile_features(RR64MK64ItemNativeSmoke PRIVATE cxx_std_20 c_std_17)
target_compile_definitions(RR64MK64ItemNativeSmoke PRIVATE RR64_EXPERIMENTAL_COURSE=1 NOMINMAX)
target_link_libraries(RR64MK64ItemNativeSmoke PRIVATE Threads::Threads)
target_include_directories(RR64MK64ItemNativeSmoke PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src" "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include" "${N64MODERN_RUNTIME_ROOT}/thirdparty")
if(MSVC)
    target_compile_options(RR64MK64ItemNativeSmoke PRIVATE /fp:strict)
else()
    target_compile_options(RR64MK64ItemNativeSmoke PRIVATE -fno-fast-math)
endif()
