set(_rr64_course_ai_generated "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs")
set(_rr64_course_ai_fixture "${CMAKE_CURRENT_BINARY_DIR}/rr64_course_ai_native_fixture.cpp")
add_custom_command(OUTPUT "${_rr64_course_ai_fixture}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_course_ai_fixture.py"
        "${_rr64_course_ai_generated}" "${_rr64_course_ai_fixture}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_course_ai_fixture.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
        "${_rr64_course_ai_generated}/funcs_2.c" "${_rr64_course_ai_generated}/funcs_3.c"
        "${_rr64_course_ai_generated}/funcs_4.c" "${_rr64_course_ai_generated}/funcs_10.c"
        "${_rr64_course_ai_generated}/funcs_11.c" "${_rr64_course_ai_generated}/funcs_14.c"
        "${_rr64_course_ai_generated}/funcs_24.c"
    VERBATIM)
add_executable(RR64CourseAiSmoke EXCLUDE_FROM_ALL tests/rr64_course_ai_smoke.cpp
    src/rr64_course_ai.cpp src/rr64_course_walls.cpp src/rr64_course_hazard_collision.cpp
    "${_rr64_course_ai_fixture}")
target_compile_features(RR64CourseAiSmoke PRIVATE cxx_std_20)
target_compile_definitions(RR64CourseAiSmoke PRIVATE RR64_EXPERIMENTAL_COURSE=1 NOMINMAX)
target_include_directories(RR64CourseAiSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src" "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include")
