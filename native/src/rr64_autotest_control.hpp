#pragma once
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

namespace rr64::autotest {
// Local developer input only. File I/O stays off the emulated CPU; commands
// expire even if the controlling process disappears. No game-state writes.
struct Control {
    std::uint64_t serial=0;
    std::uint16_t buttons=0;
    std::int8_t x=0,y=0;
    std::uint8_t actions=0; // bit0: manual eject; bit1: restart private sampling, once per serial
};
class ControlFile {
    std::mutex mutex_;
    Control current_{};
    std::chrono::steady_clock::time_point expires_{};
    std::string path_;
    std::jthread reader_;
public:
    ControlFile() {
        const char* enabled=std::getenv("RR64_AUTOTEST");
        const char* path=std::getenv("RR64_AUTOTEST_CONTROL");
        if(!enabled || !*enabled || *enabled=='0' || !path || !*path)return;
        path_=path;
        reader_=std::jthread([this](std::stop_token stop){
            std::uint64_t last=0;
            while(!stop.stop_requested()){
                std::ifstream file(path_);
                std::uint64_t serial=0;unsigned duration=0,buttons=0;int x=0,y=0;
                if(file >> serial >> duration >> std::hex >> buttons >> std::dec >> x >> y;
                   file && serial>last && duration>0 && duration<=30000 && buttons<=65535 &&
                   x>=-80 && x<=80 && y>=-80 && y<=80){
                    unsigned actions=0;
                    // Optional sixth field preserves old controller-only commands.
                    file >> std::ws;
                    if(!file.eof() && !(file >> actions)) actions=256;
                    if(actions<=3){
                    std::lock_guard lock(mutex_);
                    current_={serial,static_cast<std::uint16_t>(buttons),static_cast<std::int8_t>(x),static_cast<std::int8_t>(y),static_cast<std::uint8_t>(actions)};
                    expires_=std::chrono::steady_clock::now()+std::chrono::milliseconds(duration);
                    last=serial;
                    std::fprintf(stderr,"[RR64-AUTOTEST-CONTROL] serial=%llu ms=%u buttons=%04x stick=%d/%d actions=%u\n",
                        static_cast<unsigned long long>(serial),duration,buttons,x,y,actions);
                    }
                }
                file.close();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }
    bool enabled()const{return !path_.empty();}
    Control sample(){
        std::lock_guard lock(mutex_);
        return std::chrono::steady_clock::now()<expires_?current_:Control{};
    }
};
}
