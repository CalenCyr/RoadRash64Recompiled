# Uses the user's private supported ROM only at execution time. No game window
# or audio device is opened. Require actual generated production hook placement.
set(_rr64_audio_generated "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs")
set(_rr64_audio_fixture "${CMAKE_CURRENT_BINARY_DIR}/rr64_online_audio_native_fixture.cpp")
add_custom_command(OUTPUT "${_rr64_audio_fixture}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_online_audio_fixture.py"
        "${_rr64_audio_generated}" "${_rr64_audio_fixture}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_online_audio_fixture.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rr64_rival_engine.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
        "${_rr64_audio_generated}/funcs_2.c" "${_rr64_audio_generated}/funcs_4.c"
        "${_rr64_audio_generated}/funcs_9.c" "${_rr64_audio_generated}/funcs_12.c"
        "${_rr64_audio_generated}/funcs_13.c" "${_rr64_audio_generated}/funcs_14.c"
        "${_rr64_audio_generated}/funcs_16.c"
    VERBATIM)
add_executable(RR64OnlineAudioSmoke EXCLUDE_FROM_ALL
    tests/rr64_online_audio_smoke.cpp src/rr64_online_audio.cpp "${_rr64_audio_fixture}")
target_compile_features(RR64OnlineAudioSmoke PRIVATE cxx_std_20)
target_include_directories(RR64OnlineAudioSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src" "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include")
