#include "rr64_netplay.hpp"
#include <chrono>
#include <thread>
#include <cstdlib>
#include <string>
int main(int argc,char **argv) {
    if (argc!=4) return 2;
    const bool host=std::string(argv[1])=="host";
    rr64::netplay::Config config{};
    config.mode=host ? rr64::netplay::Mode::Host : rr64::netplay::Mode::Join;
    config.host_address="127.0.0.1"; config.port=std::atoi(argv[2]);
    rr64::netplay::configure(config);
    using Clock=std::chrono::steady_clock;
    const auto end=Clock::now()+std::chrono::seconds(12);
    auto joined=Clock::time_point{};
    while (Clock::now()<end) {
        rr64::netplay::update();
        auto s=rr64::netplay::get_status();
        if (host && s.connected_players==2) {
            if (joined==Clock::time_point{}) joined=Clock::now();
            if (Clock::now()-joined>std::chrono::milliseconds(500)) {
                if (std::string(argv[3])=="graceful") rr64::netplay::shutdown();
                return 0; // abrupt exit intentionally lets the client detect timeout
            }
        }
        if (!host && s.host_disconnected) {
            const bool retained=s.active && s.connected && s.local_slot==1 &&
                s.connected_players==2 && s.message=="Host Disconnected";
            rr64::netplay::shutdown();
            return retained ? 0 : 3;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    rr64::netplay::shutdown(); return 1;
}
