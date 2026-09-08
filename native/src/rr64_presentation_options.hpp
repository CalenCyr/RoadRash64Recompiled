#pragma once
#include <atomic>
#include <cstdlib>
#include <cstring>

namespace rr64::presentation_options
{
    // The frontend applies persisted values before boot. During gameplay it saves
    // changes for restart so an actor/terrain traversal cannot change policy midway.
    inline std::atomic_int max_lod{-1};
    inline std::atomic_int world_distance{-1};
    inline bool environment_enabled(const char *name)
    {
#ifdef _WIN32
        char *text = nullptr;
        size_t bytes = 0;
        const bool enabled = _dupenv_s(&text, &bytes, name) == 0 && text && std::strcmp(text, "1") == 0;
        std::free(text);
        return enabled;
#else
        const char *text = std::getenv(name);
        return text != nullptr && std::strcmp(text, "1") == 0;
#endif
    }
    inline bool enabled(const std::atomic_int &configured, bool fallback)
    {
        const int value = configured.load(std::memory_order_relaxed);
        return value < 0 ? fallback : value != 0;
    }
}
