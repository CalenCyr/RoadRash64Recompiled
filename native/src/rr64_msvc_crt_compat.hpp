#pragma once

// Minimal POSIX shims for the small set of MSVC/UCRT-only facilities
// (<share.h>, _dupenv_s, _fsopen) that the engine sources rely on. On
// Windows the real UCRT versions are used instead.

#ifdef _WIN32
#include <share.h>
#else
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using errno_t = int;

namespace rr64::crt_compat {

inline errno_t dupenv_s(char** buffer, std::size_t* numberOfElements, const char* varname)
{
    const char* value = std::getenv(varname);
    if (value == nullptr) {
        *buffer = nullptr;
        if (numberOfElements != nullptr) {
            *numberOfElements = 0;
        }
        return 0;
    }
    const std::size_t length = std::strlen(value) + 1u;
    *buffer = static_cast<char*>(std::malloc(length));
    if (*buffer == nullptr) {
        return ENOMEM;
    }
    std::memcpy(*buffer, value, length);
    if (numberOfElements != nullptr) {
        *numberOfElements = length;
    }
    return 0;
}

inline FILE* fsopen(const char* path, const char* mode, int /*shflag*/)
{
    return std::fopen(path, mode);
}

} // namespace rr64::crt_compat

constexpr int _SH_DENYNO = 0;

inline errno_t _dupenv_s(char** buffer, std::size_t* numberOfElements, const char* varname)
{
    return rr64::crt_compat::dupenv_s(buffer, numberOfElements, varname);
}

inline FILE* _fsopen(const char* path, const char* mode, int shflag)
{
    return rr64::crt_compat::fsopen(path, mode, shflag);
}

#endif
