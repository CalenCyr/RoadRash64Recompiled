# Exercise the actual native showroom calls, not direct calls to skin helpers.
set(_skin_preview_generated "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs")
file(GLOB _skin_preview_functions "${_skin_preview_generated}/funcs_*.c")
foreach(_variant Smoke MissingHookNegative)
    set(_preview_fixture "${CMAKE_CURRENT_BINARY_DIR}/rr64_rider_skin_preview_${_variant}.c")
    set(_preview_options)
    if(_variant STREQUAL MissingHookNegative)
        list(APPEND _preview_options --without-rider-preview-hooks)
    endif()
    add_custom_command(OUTPUT "${_preview_fixture}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_rider_skin_preview_fixture.py"
            "${_skin_preview_generated}" "${_preview_fixture}" ${_preview_options}
        DEPENDS scripts/extract_rider_skin_preview_fixture.py ${_skin_preview_functions}
        VERBATIM)
    set(_target RR64RiderSkinPreview${_variant})
    add_executable(${_target} EXCLUDE_FROM_ALL tests/rr64_rider_skin_preview_smoke.cpp
        src/rr64_rider_skins.cpp src/rr64_rider_skin_menu.cpp
        src/rr64_rider_skin_preferences.cpp src/rr64_rider_skin_render.cpp "${_preview_fixture}")
    target_include_directories(${_target} PRIVATE src tests
        $<TARGET_PROPERTY:RR64WeaponRenderSmoke,INCLUDE_DIRECTORIES>)
    target_compile_features(${_target} PRIVATE cxx_std_20 c_std_17)
    target_link_libraries(${_target} PRIVATE Threads::Threads)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /fp:strict)
    else()
        target_compile_options(${_target} PRIVATE -fno-fast-math -ffp-contract=off)
    endif()
endforeach()
