# Exercise the production UI-update function with ROM-free UI/network doubles.
# Extract the complete function, rather than a duplicate of its visibility rule,
# so a misplaced early return or transition guard fails the regression fixture.
set(_rr64_online_menu_source "${CMAKE_CURRENT_SOURCE_DIR}/src/rr64_online_menu.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_rr64_online_menu_source}")
file(READ "${_rr64_online_menu_source}" _rr64_online_menu_text)
string(FIND "${_rr64_online_menu_text}" "void update_ui() {" _rr64_online_update_begin)
string(FIND "${_rr64_online_menu_text}" "\nbool controls_online_players()" _rr64_online_update_end)
if(_rr64_online_update_begin LESS 0 OR _rr64_online_update_end LESS _rr64_online_update_begin)
    message(FATAL_ERROR "Cannot extract the production online UI update fixture")
endif()
math(EXPR _rr64_online_update_size "${_rr64_online_update_end} - ${_rr64_online_update_begin}")
string(SUBSTRING "${_rr64_online_menu_text}" ${_rr64_online_update_begin} ${_rr64_online_update_size} _rr64_online_update)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rr64_online_menu_update_fixture.inc" "${_rr64_online_update}")
# Follow the local-entry decision through the real guest device query and
# selected-profile resolver. The fixture supplies hardware/UI doubles only.
function(rr64_extract_local_input_fixture source begin_marker end_marker output)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    file(READ "${source}" _text)
    string(FIND "${_text}" "${begin_marker}" _begin)
    string(FIND "${_text}" "${end_marker}" _end)
    if(_begin LESS 0 OR _end LESS _begin)
        message(FATAL_ERROR "Cannot extract production local-input fixture: ${source}")
    endif()
    math(EXPR _size "${_end} - ${_begin}")
    string(SUBSTRING "${_text}" ${_begin} ${_size} _body)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${output}" "${_body}")
endfunction()
rr64_extract_local_input_fixture("${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "ultramodern::input::connected_device_info_t get_connected_device_info("
    "void apply_responsive_menu_navigation("
    "rr64_local_device_query_fixture.inc")
rr64_extract_local_input_fixture("${CMAKE_CURRENT_SOURCE_DIR}/lib/RecompFrontend/recompinput/src/profiles.cpp"
    "int profiles::get_game_input_profile_for_player("
    "int profiles::get_controller_event_profile("
    "rr64_local_profile_query_fixture.inc")
add_executable(RR64OnlineMenuRefreshSmoke EXCLUDE_FROM_ALL tests/rr64_online_menu_refresh_smoke.cpp)
target_include_directories(RR64OnlineMenuRefreshSmoke PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(RR64OnlineMenuRefreshSmoke PRIVATE cxx_std_20)
