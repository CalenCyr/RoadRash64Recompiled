set(_rr64_material_generated "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs")
set(_rr64_material_fixture "${CMAKE_CURRENT_BINARY_DIR}/rr64_mk64_item_material_call_fixture.c")
set(_rr64_material_resolver "${CMAKE_CURRENT_BINARY_DIR}/rr64_mk64_item_material_call_fixture.resolver.cpp")
file(GLOB _rr64_material_functions "${_rr64_material_generated}/funcs_*.c")
add_custom_command(OUTPUT "${_rr64_material_fixture}" "${_rr64_material_resolver}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_mk64_item_material_call_fixture.py"
        "${_rr64_material_generated}" "${_rr64_material_fixture}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml" "${CMAKE_CURRENT_SOURCE_DIR}/lib/rt64"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_mk64_item_material_call_fixture.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/rt64/src/hle/rt64_rsp.cpp" ${_rr64_material_functions}
    VERBATIM)
add_executable(RR64Mk64ItemMaterialCallSmoke EXCLUDE_FROM_ALL
    tests/rr64_mk64_item_material_call_smoke.cpp src/rr64_mk64_item_material.cpp
    "${_rr64_material_fixture}" "${_rr64_material_resolver}")
target_include_directories(RR64Mk64ItemMaterialCallSmoke PRIVATE src
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include" "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include")
target_compile_features(RR64Mk64ItemMaterialCallSmoke PRIVATE cxx_std_20 c_std_17)
target_compile_definitions(RR64Mk64ItemMaterialCallSmoke PRIVATE RR64_EXPERIMENTAL_COURSE=1)
if(MSVC)
    target_compile_options(RR64Mk64ItemMaterialCallSmoke PRIVATE /fp:strict)
else()
    target_compile_options(RR64Mk64ItemMaterialCallSmoke PRIVATE -fno-fast-math -ffp-contract=off)
endif()
