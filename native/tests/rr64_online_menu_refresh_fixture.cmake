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
add_executable(RR64OnlineMenuRefreshSmoke EXCLUDE_FROM_ALL tests/rr64_online_menu_refresh_smoke.cpp)
target_include_directories(RR64OnlineMenuRefreshSmoke PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(RR64OnlineMenuRefreshSmoke PRIVATE cxx_std_20)
