if(UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Keep the existing option name for build-script compatibility. Players install
# the optional tracks from their own ROM; enabling support embeds no track data.
option(RR64_EXPERIMENTAL_COURSE "Enable optional user-ROM course packs and local import" ON)

if(NOT MSVC)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:C>:-include;stdint.h>"
        "$<$<COMPILE_LANGUAGE:CXX>:-include;cstdint>"
    )
endif()


if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Road Rash 64 Recompiled requires a 64-bit build.")
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/bin")
endforeach()

set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(N64MODERN_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/N64ModernRuntime")
set(RT64_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/rt64")
set(RECOMP_FRONTEND_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/RecompFrontend")
set(RECOMPILED_DIR "${ROOT_DIR}/build/RecompiledFuncs")
