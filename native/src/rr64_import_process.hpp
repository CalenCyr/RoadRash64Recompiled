#pragma once

#include <filesystem>
#include <functional>
#include <stop_token>
#include <vector>

namespace rr64::mk64_import {
struct ProcessResult { int exit_code; bool cancelled; };

// Runs the bundled converter without a command shell. Output is a local log;
// tick runs on the worker, never on the UI or game/audio/render threads.
ProcessResult run_process(const std::filesystem::path& executable,
    const std::vector<std::filesystem::path>& arguments,
    const std::filesystem::path& log, const std::filesystem::path& cancel_file,
    std::stop_token stop, const std::function<void()>& tick);
}
