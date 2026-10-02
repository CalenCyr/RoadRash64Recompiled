# Uses the production Config parser/serializer/file backup implementation.
# Run with one argument: a new, nonexistent disposable output directory.
# Then run --verify-off with that directory in a fresh process to check restart.
add_executable(RR64RivalEngineConfigSmoke EXCLUDE_FROM_ALL
    tests/rr64_rival_engine_config_smoke.cpp
    src/rr64_rival_engine_config.cpp
    "${N64MODERN_RUNTIME_ROOT}/librecomp/src/config.cpp"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/src/config_option.cpp"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/src/files.cpp")
target_compile_features(RR64RivalEngineConfigSmoke PRIVATE cxx_std_20)
target_include_directories(RR64RivalEngineConfigSmoke BEFORE PRIVATE
    "${N64MODERN_RUNTIME_ROOT}/thirdparty"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include"
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include/librecomp"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include"
    "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/thirdparty/concurrentqueue")
# miniz supplies its generated export header as well as the public headers.
target_link_libraries(RR64RivalEngineConfigSmoke PRIVATE miniz Threads::Threads)
if(WIN32)
    target_compile_definitions(RR64RivalEngineConfigSmoke PRIVATE NOMINMAX)
endif()
