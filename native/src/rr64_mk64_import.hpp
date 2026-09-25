#pragma once
#include <filesystem>
#include <string>

namespace rr64::mk64_import {
struct Status {
    bool busy = false;
    bool can_cancel = false;
    bool tool_available = false;
    std::string message;
};
// Configure/register before the launcher is created. Converter resources are
// application tools; ROMs and generated assets remain in user-owned storage.
void configure(std::filesystem::path converter, std::filesystem::path rr64_rom);
Status status();
bool busy() noexcept;
void start(const std::filesystem::path& mk64_rom);
void cancel() noexcept;
// UI thread: releases the pack lease and reports a successful install once.
bool update();
void shutdown() noexcept;
}
