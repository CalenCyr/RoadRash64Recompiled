#include "rr64_import_process.hpp"

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace rr64::mk64_import {
namespace {
using Clock = std::chrono::steady_clock;
#ifdef _WIN32
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

// Microsoft argv quoting: a run of backslashes is doubled only before a quote
// or the closing quote. ROM paths are data, including spaces and punctuation.
std::wstring quote(const std::filesystem::path& path) {
    std::wstring result = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : path.native()) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        result.push_back(c);
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}

class Child {
    Handle log_handle, job, process, thread;
public:
    Child(const std::filesystem::path& exe, const std::vector<std::filesystem::path>& args,
          const std::filesystem::path& log) {
        SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
        log_handle.value = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
            &security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (log_handle.value == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot create the import log. Check the installation folder is writable.");
        job.value = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
            throw std::runtime_error("Cannot prepare the converter process.");

        // Only this log handle is inherited. Game/network/device handles must
        // not leak into a potentially long conversion or delay application exit.
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        std::vector<unsigned char> storage(bytes);
        auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes))
            throw std::runtime_error("Cannot prepare converter handle isolation.");
        struct Attributes { LPPROC_THREAD_ATTRIBUTE_LIST p; ~Attributes() { DeleteProcThreadAttributeList(p); } } cleanup{attributes};
        if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                      &log_handle.value, sizeof(HANDLE), nullptr, nullptr))
            throw std::runtime_error("Cannot isolate the converter handles.");
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdOutput = log_handle.value;
        startup.StartupInfo.hStdError = log_handle.value;
        startup.StartupInfo.hStdInput = nullptr;
        startup.lpAttributeList = attributes;
        std::wstring command = quote(exe);
        for (const auto& arg : args) command += L" " + quote(arg);
        PROCESS_INFORMATION info{};
        const auto cwd = exe.parent_path();
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
                nullptr, cwd.c_str(), &startup.StartupInfo, &info))
            throw std::runtime_error("Cannot start the bundled MK64 importer. Re-extract the complete build download.");
        process.value = info.hProcess;
        thread.value = info.hThread;
        if (!AssignProcessToJobObject(job.value, process.value) || ResumeThread(thread.value) == DWORD(-1)) {
            TerminateProcess(process.value, 1);
            WaitForSingleObject(process.value, INFINITE);
            throw std::runtime_error("Cannot start the converter safely.");
        }
    }
    bool poll(int& code) {
        if (WaitForSingleObject(process.value, 0) != WAIT_OBJECT_0) return false;
        DWORD status = 1;
        GetExitCodeProcess(process.value, &status);
        code = static_cast<int>(status);
        return true;
    }
    void terminate() { TerminateJobObject(job.value, 1); }
};
#else
class Child {
    pid_t pid = -1;
    bool reaped = false;
public:
    Child(const std::filesystem::path& exe, const std::vector<std::filesystem::path>& args,
          const std::filesystem::path& log) {
        const int fd = open(log.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0) throw std::runtime_error("Cannot create the import log. Check the installation folder is writable.");
        struct File { int fd; ~File() { close(fd); } } close_log{fd};
        posix_spawn_file_actions_t actions;
        posix_spawnattr_t attributes;
        if (posix_spawn_file_actions_init(&actions) != 0)
            throw std::runtime_error("Cannot prepare the converter process.");
        struct Actions { posix_spawn_file_actions_t* p; ~Actions() { posix_spawn_file_actions_destroy(p); } } close_actions{&actions};
        if (posix_spawnattr_init(&attributes) != 0)
            throw std::runtime_error("Cannot prepare converter process attributes.");
        struct Attrs { posix_spawnattr_t* p; ~Attrs() { posix_spawnattr_destroy(p); } } close_attrs{&attributes};
        int error = posix_spawn_file_actions_adddup2(&actions, fd, STDOUT_FILENO);
        error |= posix_spawn_file_actions_adddup2(&actions, fd, STDERR_FILENO);
        error |= posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        if (fd > STDERR_FILENO) error |= posix_spawn_file_actions_addclose(&actions, fd);
#ifdef __GLIBC__
#if __GLIBC_PREREQ(2, 34)
        // Match Windows' handle allow-list on current Linux distributions:
        // only standard streams survive, even if a library forgot CLOEXEC.
        error |= posix_spawn_file_actions_addclosefrom_np(&actions, 3);
#endif
#endif
        error |= posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
        error |= posix_spawnattr_setpgroup(&attributes, 0);
        std::vector<std::string> strings{exe.string()};
        for (const auto& arg : args) strings.push_back(arg.string());
        std::vector<char*> argv;
        for (auto& arg : strings) argv.push_back(arg.data());
        argv.push_back(nullptr);
        if (error || posix_spawn(&pid, exe.c_str(), &actions, &attributes, argv.data(), environ) != 0)
            throw std::runtime_error("Cannot start the bundled MK64 importer. Re-extract the complete build download.");
    }
    ~Child() {
        // A converter may exit before one of its helpers. Keep the process
        // group lifetime bounded like the Windows kill-on-close job, even when
        // waitpid has already collected the direct child's exit status.
        if (pid > 0) {
            terminate();
            if (!reaped) {
                int status;
                while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
            }
        }
    }
    bool poll(int& code) {
        int status;
        const auto result = waitpid(pid, &status, WNOHANG);
        if (result == 0 || (result < 0 && errno == EINTR)) return false;
        reaped = true;
        code = result < 0 ? 1 : WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        return true;
    }
    void terminate() { if (pid > 0) kill(-pid, SIGKILL); }
};
#endif
}

ProcessResult run_process(const std::filesystem::path& executable,
    const std::vector<std::filesystem::path>& arguments,
    const std::filesystem::path& log, const std::filesystem::path& cancel_file,
    std::stop_token stop, const std::function<void()>& tick) {
    if (stop.stop_requested()) return {1, true};
    Child child(executable, arguments, log);
    bool cancelling = false, terminated = false;
    Clock::time_point deadline;
    int code = 1;
    while (!child.poll(code)) {
        if (stop.stop_requested() && !cancelling) {
            cancelling = true;
            deadline = Clock::now() + std::chrono::seconds(3);
            std::ofstream(cancel_file) << "cancel\n";
        }
        if (cancelling && !terminated && Clock::now() >= deadline) {
            child.terminate();
            terminated = true;
        }
        tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return {code, cancelling || stop.stop_requested()};
}
}
