#include "rr64_autotest_control.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
void check(bool ok){if(!ok)throw std::runtime_error("autotest control check failed");}
int main(int argc,char**argv){
    check(argc==2);
    const std::filesystem::path path=argv[1];
    check(!std::filesystem::exists(path));
    _putenv_s("RR64_AUTOTEST_CONTROL",path.string().c_str());
    _putenv_s("RR64_AUTOTEST","0");
    {rr64::autotest::ControlFile disabled;check(!disabled.enabled());}
    _putenv_s("RR64_AUTOTEST","1");
    const auto write=[&](const char* text){
        for(int tries=0;tries<50;++tries){
            {std::ofstream out(path);if(out){out<<text;return;}}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("cannot write test input");
    };
    {
        rr64::autotest::ControlFile control;check(control.enabled());
        auto await_serial=[&](unsigned serial){
            for(int i=0;i<100;++i){
                auto sample=control.sample();if(sample.serial==serial)return sample;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            throw std::runtime_error("input reader timeout");
        };
        write("1 200 2000 -15 20\n");
        auto first=await_serial(1);check(first.buttons==0x2000 && first.x==-15 && first.y==20 && !first.actions);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        check(control.sample().serial==0); // stale file cannot re-arm expired input
        write("2 500 0000 0 0 4\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        check(control.sample().serial==0); // unsupported action rejected
        write("2 500 0000 0 0 1\n");
        check(await_serial(2).actions==1); // rejected serial was not consumed
        write("3 500 0000 81 0 1\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        check(control.sample().serial==2); // malformed command cannot replace valid one
        write("3 50 0000 0 0 0\n");check(!await_serial(3).actions);
        write("4 500 0000 0 0 broken\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        check(control.sample().serial==0);
        write("4 500 0000 0 0 2\n");check(await_serial(4).actions==2);
        write("5 500 0000 0 0 3\n");check(await_serial(5).actions==3);
    } // stop and join the reader before releasing its state
    std::filesystem::remove(path);
    std::cout<<"Autotest controls passed: disabled path, legacy input, expiry, rejection, action, shutdown\n";
}
