add_executable(RR64DiagnosticOptionsSmoke EXCLUDE_FROM_ALL
    tests/rr64_diagnostic_options_smoke.cpp)
target_compile_features(RR64DiagnosticOptionsSmoke PRIVATE cxx_std_17)
target_include_directories(RR64DiagnosticOptionsSmoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(RR64DiagnosticOptionsSmoke PRIVATE Threads::Threads)

add_executable(RR64SchedulerDiagnosticsSmoke EXCLUDE_FROM_ALL
    tests/rr64_scheduler_diagnostics_smoke.cpp)
target_compile_features(RR64SchedulerDiagnosticsSmoke PRIVATE cxx_std_17)
target_include_directories(RR64SchedulerDiagnosticsSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include")
