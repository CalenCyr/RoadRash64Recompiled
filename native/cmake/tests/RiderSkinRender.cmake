add_executable(RR64RiderSkinRenderSmoke EXCLUDE_FROM_ALL
    tests/rr64_rider_skin_render_smoke.cpp src/rr64_rider_skins.cpp
    src/rr64_rider_skin_render.cpp src/rr64_mk64_item_material.cpp
    "${_rr64_material_fixture}" "${_rr64_material_resolver}")
target_include_directories(RR64RiderSkinRenderSmoke PRIVATE src
    $<TARGET_PROPERTY:RR64Mk64ItemMaterialCallSmoke,INCLUDE_DIRECTORIES>)
target_compile_features(RR64RiderSkinRenderSmoke PRIVATE cxx_std_20 c_std_17)
target_compile_definitions(RR64RiderSkinRenderSmoke PRIVATE RR64_EXPERIMENTAL_COURSE=1)
target_link_libraries(RR64RiderSkinRenderSmoke PRIVATE Threads::Threads)
