#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
struct Achievement { unsigned retro_id; };
constexpr std::array<Achievement,3> kAchievements{{{101},{102},{103}}};
std::mutex g_mutex, io_mutex;
std::condition_variable io_cv;
bool g_initialized=false,g_persist_dirty=true;
std::array<bool,3> g_unlocked{true,false,false};
std::filesystem::path g_progress_path;
bool block_io=false,entered=false,release_io=false,fail_io=false;
unsigned writes=0;
std::array<bool,3> saved{};
bool save_progress(const std::array<bool,3>& value) {
    std::unique_lock lock(io_mutex);
    ++writes;entered=true;io_cv.notify_all();
    if(block_io) io_cv.wait(lock,[]{return release_io;});
    saved=value;
    return !fail_io;
}
// Extracted production snapshot/dirty handling and filesystem writer.
#include "rr64_achievement_flush.hpp"
void check(bool value){if(!value)std::abort();}
int main(){
    flush_progress();check(writes==0 && g_persist_dirty);
    g_initialized=true;block_io=true;
    std::thread worker(flush_progress);
    {std::unique_lock lock(io_mutex);check(io_cv.wait_for(lock,std::chrono::seconds(2),[]{return entered;}));}
    // Producers must remain responsive while the filesystem is blocked.
    check(g_mutex.try_lock());g_unlocked[1]=true;g_persist_dirty=true;g_mutex.unlock();
    {std::lock_guard lock(io_mutex);release_io=true;io_cv.notify_all();}
    worker.join();check(saved[0] && !saved[1] && g_persist_dirty);
    block_io=false;flush_progress();check(saved[1] && !g_persist_dirty && writes==2);
    flush_progress();check(writes==2);
    g_unlocked[2]=true;g_persist_dirty=true;fail_io=true;
    flush_progress();check(g_persist_dirty);
    fail_io=false;flush_progress();check(!g_persist_dirty && saved[2]);
    // Check the actual writer's format and error return in a private directory.
    const auto root=std::filesystem::temp_directory_path()/
        ("rr64-progress-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    g_progress_path=root/"achievements.txt";
    check(disk_save_progress({true,false,true}));
    std::ifstream input(g_progress_path);std::string text((std::istreambuf_iterator<char>(input)),{});input.close();
    check(text=="# Road Rash 64 Recompiled local achievements v1\n101\n103\n");
    g_progress_path=root;check(!disk_save_progress({}));
    std::filesystem::remove(root/"achievements.txt");std::filesystem::remove(root);
    std::puts("Achievement persistence: concurrency, retry, coalescing, format and failure checks passed");
}
