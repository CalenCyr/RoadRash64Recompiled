set(RR64_MK64_INPUT_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/rr64_mk64_item_input_fixture.inc")
add_custom_command(OUTPUT "${RR64_MK64_INPUT_FIXTURE}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_mk64_item_input_fixture.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${RR64_MK64_INPUT_FIXTURE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_mk64_item_input_fixture.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/rr64_runtime_shims.cpp"
    VERBATIM)
set_source_files_properties("${RR64_MK64_INPUT_FIXTURE}" PROPERTIES HEADER_FILE_ONLY TRUE)
add_executable(RR64Mk64ItemInputSmoke EXCLUDE_FROM_ALL
    tests/rr64_mk64_item_input_smoke.cpp "${RR64_MK64_INPUT_FIXTURE}")
target_include_directories(RR64Mk64ItemInputSmoke PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    $<TARGET_PROPERTY:RR64NetplaySmoke,INCLUDE_DIRECTORIES>)
target_compile_definitions(RR64Mk64ItemInputSmoke PRIVATE RR64_EXPERIMENTAL_COURSE=1)
target_compile_features(RR64Mk64ItemInputSmoke PRIVATE cxx_std_20)
