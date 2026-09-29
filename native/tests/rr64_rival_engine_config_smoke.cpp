#include "rr64_rival_engine_config.hpp"
#include "librecomp/config.hpp"
#include "librecomp/game.hpp"
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <utility>

namespace {
std::filesystem::path directory;
double observed_volume=-1;
unsigned checks=0;
void check(bool condition,const char *message) {
    ++checks;
    if(!condition){std::cerr<<"rival engine config failed: "<<message<<'\n';std::exit(1);}
}
recomp::config::Config make_config() {
    recomp::config::Config config("Audio","sound",false);
    config.add_percent_number_option("main_volume","Main Volume","",100);
    rr64::rival_engine::configure_volume(config);
    return config;
}
void write_json(const nlohmann::json &value) {
    std::ofstream output(directory/"sound.json");output<<value.dump();
}
nlohmann::json read_json() {
    nlohmann::json value;std::ifstream input(directory/"sound.json");input>>value;return value;
}
}
namespace recomp {
std::filesystem::path get_config_path(){return directory;}
const Version &get_project_version(){static const Version version(1,4,1);return version;}
}
namespace rr64::rival_engine {
// The real Config, JSON parser, callbacks and backup-file implementation run.
// The engine sink records UI delivery without starting any audio or game thread.
void set_volume_percent(double value) noexcept {observed_volume=value;}
double get_volume_percent() noexcept {return observed_volume;}
}
int main(int argc,char **argv) {
    using namespace rr64::rival_engine;
    if(argc!=2)return 2;
    directory=argv[1];
    if(std::filesystem::exists(directory) || !std::filesystem::create_directories(directory))return 3;
    auto config=make_config();
    const auto &schema=std::get<recomp::config::ConfigOptionNumber>(config.get_option(volume_option).variant);
    check(schema.min==0 && schema.max==100 && schema.step==1 && schema.precision==0 && schema.percent,"native percentage slider range");
    check(schema.default_value==35,"conservative default");
    check(config.get_option(volume_option).description.find("including online players")!=std::string::npos,"opponent scope described");
    check(config.load_config(),"first-run config saved");
    apply_volume_config(config);
    check(observed_volume==35 && read_json()[volume_option]==35,"first-run default persisted and applied");

    write_json({{"main_volume",62}});
    auto old=make_config();check(old.load_config(),"old config loaded");
    check(observed_volume==35,"missing option uses default at load");
    check(std::get<double>(old.get_option_value("main_volume"))==62,"existing main volume preserved");
    check(old.save_config() && read_json()[volume_option]==35,"old config gains saved default");
    for(const auto &[input,expected]:std::array<std::pair<double,double>,5>{{{-40,0},{140,100},{0,0},{100,100},{47,47}}}) {
        write_json({{"main_volume",62},{volume_option,input}});
        auto loaded=make_config();check(loaded.load_config(),"numeric config loaded");
        check(observed_volume==expected && std::get<double>(loaded.get_option_value(volume_option))==expected,"loaded value clamped before UI and runtime");
        check(loaded.save_config() && read_json()[volume_option]==expected,"normalized value persisted");
    }
    for(const auto &invalid:std::array<nlohmann::json,4>{nullptr,"loud",true,nlohmann::json::array({1})}) {
        write_json({{volume_option,invalid}});
        auto loaded=make_config();check(loaded.load_config() && observed_volume==35,"malformed option falls back safely");
    }
    auto changed=make_config();
    check(changed.load_config(),"UI config loaded");
    for(double value:{0.0,73.0,100.0}) {
        changed.set_option_value(volume_option,value);
        check(observed_volume==value,"slider update reaches engine immediately including off");
        check(changed.save_config() && read_json()[volume_option]==value,"slider update saved");
        auto restarted=make_config();check(restarted.load_config() && observed_volume==value,"slider setting survives new Config instance");
    }
    changed.set_option_value(volume_option,std::numeric_limits<double>::quiet_NaN());
    check(observed_volume==35 && changed.save_config() && read_json()[volume_option]==35,"nonfinite programmatic value cannot reach audio or JSON");
    check(std::filesystem::exists(directory/"sound.json.bak"),"normal runtime backup retained");
    std::cout<<"rival engine config smoke passed "<<checks<<" checks\n";
}
