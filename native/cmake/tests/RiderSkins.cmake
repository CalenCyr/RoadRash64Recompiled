# Optional authored textures retain the original native models and animations.
add_executable(RR64RiderSkinMenuSmoke EXCLUDE_FROM_ALL
    tests/rr64_rider_skin_menu_smoke.cpp src/rr64_rider_skins.cpp
    src/rr64_rider_skin_menu.cpp src/rr64_rider_skin_preferences.cpp)
target_include_directories(RR64RiderSkinMenuSmoke PRIVATE src tests
    $<TARGET_PROPERTY:RR64WeaponRenderSmoke,INCLUDE_DIRECTORIES>)
target_link_libraries(RR64RiderSkinMenuSmoke PRIVATE Threads::Threads)
target_compile_features(RR64RiderSkinMenuSmoke PRIVATE cxx_std_20)

add_executable(RR64RiderSkinPreferencesSmoke EXCLUDE_FROM_ALL
    tests/rr64_rider_skin_preferences_smoke.cpp src/rr64_rider_skin_preferences.cpp)
target_include_directories(RR64RiderSkinPreferencesSmoke PRIVATE src)
target_link_libraries(RR64RiderSkinPreferencesSmoke PRIVATE Threads::Threads)
target_compile_features(RR64RiderSkinPreferencesSmoke PRIVATE cxx_std_20)

# Use the runtime's actual file handles; only running-mod-manager callbacks are
# substituted. Bounded ZIP parsing never depends on a game or a renderer.
set(_skin_manifest "${CMAKE_CURRENT_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/src/mod_manifest.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_skin_manifest}")
file(READ "${_skin_manifest}" _skin_manifest_text)
string(FIND "${_skin_manifest_text}" "const std::string game_mod_id_key" _skin_prefix_end)
if(_skin_prefix_end LESS 0)
    message(FATAL_ERROR "Cannot extract production mod handles for skin fixture")
endif()
string(SUBSTRING "${_skin_manifest_text}" 0 ${_skin_prefix_end} _skin_handles)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rr64_rider_skin_file_handles.cpp" "${_skin_handles}")
add_executable(RR64RiderSkinArchiveSmoke EXCLUDE_FROM_ALL
    tests/rr64_rider_skin_archive_smoke.cpp src/rr64_rider_skins.cpp src/rr64_rider_skin_mod_ui.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/rr64_rider_skin_file_handles.cpp")
target_include_directories(RR64RiderSkinArchiveSmoke PRIVATE src tests
    "${N64MODERN_RUNTIME_ROOT}/librecomp/include" "${N64MODERN_RUNTIME_ROOT}/ultramodern/include"
    "${N64MODERN_RUNTIME_ROOT}/N64Recomp/include" "${N64MODERN_RUNTIME_ROOT}/thirdparty"
    "${N64MODERN_RUNTIME_ROOT}/thirdparty/concurrentqueue")
target_link_libraries(RR64RiderSkinArchiveSmoke PRIVATE miniz Threads::Threads)
target_compile_features(RR64RiderSkinArchiveSmoke PRIVATE cxx_std_20)
foreach(target RR64CampaignBikeSmoke RR64CampaignBonusSaveSmoke)
    if(TARGET ${target})
        target_sources(${target} PRIVATE tests/rider_skins_disabled.cpp)
    endif()
endforeach()
