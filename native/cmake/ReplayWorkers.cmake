set(RR64_REPLAY_RESOURCE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/rr64_replay_resource_worker.cpp")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(RR64_REPLAY_FRAME_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/rr64_replay_native_frame.cpp")
file(GLOB RR64_REPLAY_INPUTS "${RECOMPILED_DIR}/funcs_*.c")
add_custom_command(OUTPUT "${RR64_REPLAY_FRAME_SOURCE}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_prediction_frame.py"
        "${RECOMPILED_DIR}" "${RR64_REPLAY_FRAME_SOURCE}"
    DEPENDS ${RR64_REPLAY_INPUTS} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_prediction_frame.py"
    VERBATIM)
add_custom_command(OUTPUT "${RR64_REPLAY_RESOURCE_SOURCE}"
    COMMAND "${CMAKE_COMMAND}" "-DINPUT=${RECOMPILED_DIR}/funcs_1.c"
        "-DOUTPUT=${RR64_REPLAY_RESOURCE_SOURCE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateReplayResourceWorker.cmake"
    DEPENDS "${RECOMPILED_DIR}/funcs_1.c" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateReplayResourceWorker.cmake"
    VERBATIM)
