set(_rr64_bonus_save_generated "${CMAKE_CURRENT_SOURCE_DIR}/../build/RecompiledFuncs")
set(_rr64_bonus_save_fixture "${CMAKE_CURRENT_BINARY_DIR}/rr64_campaign_bonus_save_native_fixture.cpp")
add_custom_command(OUTPUT "${_rr64_bonus_save_fixture}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_campaign_bonus_save_fixture.py"
        "${_rr64_bonus_save_generated}" "${_rr64_bonus_save_fixture}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_campaign_bonus_save_fixture.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../config/roadrash64.us.toml"
        "${_rr64_bonus_save_generated}/funcs_6.c"
        "${_rr64_bonus_save_generated}/funcs_10.c"
        "${_rr64_bonus_save_generated}/funcs_15.c"
        "${_rr64_bonus_save_generated}/funcs_26.c"
    VERBATIM)
add_executable(RR64CampaignBonusSaveSmoke EXCLUDE_FROM_ALL
    tests/rr64_campaign_bonus_save_smoke.cpp src/rr64_campaign_bonus_save.cpp
    src/rr64_campaign_completion.cpp
    "${_rr64_bonus_save_fixture}")
target_compile_features(RR64CampaignBonusSaveSmoke PRIVATE cxx_std_20)
target_include_directories(RR64CampaignBonusSaveSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src" "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include")
