#include "rr64_mk64_import.hpp"
#include "rr64_import_install.hpp"
#include "rr64_import_process.hpp"
#include "rr64_race_pack_mod.hpp"
#include "json/json.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace rr64::mk64_import {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
enum class Completion { None, Installed, Failed, Cancelled };
// The UI thread owns worker lifetime and path configuration. The worker only
// publishes status; update() joins it before releasing the race-pack lease.
std::mutex status_mutex;
Status state;
std::filesystem::path converter_path, game_rom_path;
std::jthread worker;
std::atomic_bool running{false};
std::atomic<Completion> completion{Completion::None};

void publish(std::string message, bool cancellable) {
    std::scoped_lock lock(status_mutex);
    state.message = std::move(message);
    state.can_cancel = cancellable;
}
Json read_json(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error || size > 65536) return {};
    std::ifstream input(path, std::ios::binary);
    return input ? Json::parse(input, nullptr, false) : Json{};
}
std::string plain_text(std::string text) {
    // Mod descriptions support markup. Converter progress/errors are data,
    // including messages derived from a selected filename, not UI markup.
    std::string result;
    for (unsigned char c : text) {
        if (result.size() >= 500) break;
        switch (c) {
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '&': result += "&amp;"; break;
        default: if (c >= 32 || c == '\n') result += char(c); break;
        }
    }
    return result;
}
void update_progress(const fs::path& progress, std::stop_token stop) {
    if (stop.stop_requested()) return;
    const auto value = read_json(progress);
    if (!value.is_object()) return; // Atomic replacement may be between frames.
    const auto message = value.find("message"), percent = value.find("percent");
    if (message == value.end() || !message->is_string()) return;
    std::string text = "Importing: " + plain_text(message->get<std::string>());
    if (percent != value.end() && percent->is_number()) {
        const double p = percent->get<double>();
        if (p >= 0 && p <= 100) text += " (" + std::to_string(static_cast<unsigned>(p)) + "%)";
    }
    publish(std::move(text), true);
}
void verify_pack_report(const fs::path& pack, const Json& report, std::stop_token stop) {
    // Progress is advisory. Installation requires the completed catalogue and
    // every member file to match the converter's final integrity report.
    const auto digest = validate_pack(pack, stop);
    if (report.at("catalogue_sha256") != digest ||
        !report.contains("converter_version") || !report["converter_version"].is_string() ||
        report["converter_version"].get<std::string>().empty())
        throw std::runtime_error("The converter's final integrity report does not match its files.");
}
fs::path make_job_directory(const fs::path& destination) {
    fs::create_directories(destination.parent_path());
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        const auto path = destination.parent_path() /
            (".mk64-import-" + std::to_string(stamp) + "-" + std::to_string(attempt));
        if (fs::create_directory(path)) return path;
    }
    throw std::runtime_error("Cannot create a temporary import folder.");
}
void convert(std::stop_token stop, fs::path selected_rom, fs::path destination) {
    auto result = Completion::Failed;
    fs::path job;
    bool committing = false;
    try {
        if (!fs::is_regular_file(game_rom_path))
            throw std::runtime_error("Select your Road Rash 64 ROM from the launcher first, then import your Mario Kart 64 ROM here.");
        if (!fs::is_regular_file(selected_rom))
            throw std::runtime_error("The selected Mario Kart 64 ROM could not be opened.");
        job = make_job_directory(destination);
        const auto pack = job / "pack";
        const auto progress = job / "progress.json";
        const auto process = run_process(converter_path,
            {"--mk64-rom", selected_rom, "--rr64-rom", game_rom_path,
             "--output", pack, "--progress", progress, "--cancel-file", job / "cancel"},
            job / "converter.log", job / "cancel", stop, [&] {
                update_progress(progress, stop);
            });
        if (process.cancelled || stop.stop_requested()) {
            result = Completion::Cancelled;
            publish("Import cancelled. The previous pack is unchanged.", false);
        } else {
            const auto report = read_json(job / "result.json");
            if (process.exit_code != 0 || !report.is_object() || !report.value("success", false)) {
                std::string error = "The MK64 import failed. The previous pack is unchanged. Details are in the import folder's converter.log.";
                if (report.is_object() && report.contains("error") && report["error"].is_string())
                    error = report["error"].get<std::string>() + " The previous pack is unchanged.";
                throw std::runtime_error(error);
            }
            publish("Checking all sixteen courses, sounds and file integrity...", true);
            verify_pack_report(pack, report, stop);
            if (stop.stop_requested()) {
                result = Completion::Cancelled;
                publish("Import cancelled. The previous pack is unchanged.", false);
            } else {
                // Cancellation stops before this short transaction. Once the
                // previous folder is renamed, complete install or roll back.
                publish("Installing the verified pack...", false);
                committing = true;
                install_pack(pack, destination, job / "previous-pack");
                publish("Import complete. Tracks are ready; use the mod toggle to enable or disable them before starting.", false);
                result = Completion::Installed;
            }
        }
    } catch (const std::exception& error) {
        const bool cancelled = stop.stop_requested() && !committing;
        result = cancelled ? Completion::Cancelled : Completion::Failed;
        publish(cancelled ? "Import cancelled. The previous pack is unchanged." : plain_text(error.what()), false);
        if (!job.empty()) {
            std::ofstream log(job / "installation-error.txt");
            log << error.what() << '\n';
        }
    } catch (...) {
        publish("The import could not finish. The previous pack is preserved. Please keep the import log for diagnosis.", false);
    }
    completion.store(result, std::memory_order_release);
}
}

void configure(fs::path converter, fs::path rr64_rom) {
    if (running.load(std::memory_order_acquire)) return;
    converter_path = std::move(converter);
    game_rom_path = std::move(rr64_rom);
    std::error_code error;
    std::scoped_lock lock(status_mutex);
    state.tool_available = fs::is_regular_file(converter_path, error) && !error;
}

Status status() {
    std::scoped_lock lock(status_mutex);
    auto snapshot = state;
    snapshot.busy = running.load(std::memory_order_acquire);
    return snapshot;
}
bool busy() noexcept { return running.load(std::memory_order_acquire); }

void start(const fs::path& mk64_rom) {
    if (busy()) return;
    if (!status().tool_available) {
        publish("The bundled MK64 importer is missing. Re-extract the complete build download.", false);
        return;
    }
    fs::path destination;
    if (!race_pack_mod::begin_import(destination)) {
        publish("Restart the application before importing tracks, and leave any online session first.", false);
        return;
    }
    running.store(true, std::memory_order_release);
    completion.store(Completion::None, std::memory_order_release);
    publish("Preparing your local ROM import...", true);
    try {
        // Copy paths before starting. A selected ROM is read-only, never moved
        // into the pack and never included in the generated mod files.
        worker = std::jthread(convert, fs::absolute(mk64_rom), fs::absolute(destination));
    } catch (...) {
        race_pack_mod::end_import();
        running.store(false, std::memory_order_release);
        publish("Could not start the importer. The previous pack is unchanged.", false);
    }
}

void cancel() noexcept {
    std::scoped_lock lock(status_mutex);
    if (running.load(std::memory_order_acquire) && state.can_cancel &&
        completion.load(std::memory_order_acquire) == Completion::None) {
        worker.request_stop();
        state.message = "Cancelling the import...";
        state.can_cancel = false;
    }
}
bool update() {
    const auto done = completion.exchange(Completion::None, std::memory_order_acq_rel);
    if (done == Completion::None) return false;
    if (worker.joinable()) worker.join();
    race_pack_mod::end_import();
    running.store(false, std::memory_order_release);
    return done == Completion::Installed;
}
void shutdown() noexcept {
    if (worker.joinable()) {
        worker.request_stop();
        worker.join();
    }
    if (running.exchange(false)) race_pack_mod::end_import();
    completion.store(Completion::None);
}
}
