cmake_minimum_required(VERSION 3.20)
project(RR64MK64Contact LANGUAGES C CXX)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(rr64-mk64-contact EXCLUDE_FROM_ALL contact.cpp native_contact.c
    "../../native/src/rr64_experimental_course_policy.cpp")
target_compile_definitions(rr64-mk64-contact PRIVATE RR64_EXPERIMENTAL_COURSE=1)
target_include_directories(rr64-mk64-contact PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../native/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../native/lib/N64ModernRuntime/N64Recomp/include")
if(MSVC)
    # The importer helper must run on a clean machine without an extra CRT install.
    set_property(TARGET rr64-mk64-contact PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(rr64-mk64-contact PRIVATE /fp:strict /D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(rr64-mk64-contact PRIVATE -ffp-contract=off -frounding-math)
    target_link_libraries(rr64-mk64-contact PRIVATE m)
endif()
