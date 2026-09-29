#include "rr64_import_install.hpp"
#include "json/json.hpp"
#include <array>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    const std::filesystem::path pack = argv[1], scratch = argv[2];
    const auto identity = rr64::mk64_import::validate_pack(pack, {});
    std::ifstream original(pack / "catalogue.json");
    nlohmann::json catalogue;
    original >> catalogue;
    if (catalogue.at("converter_version") != "1.0.1-c40" || catalogue.at("mk64_items_version") != 1)
        return 3;
    if (std::filesystem::exists(scratch)) return 4;
    std::filesystem::create_directories(scratch);
    for (unsigned missing = 0; missing < 3; ++missing) {
        auto legacy = catalogue;
        legacy.erase(std::array{"mk64_items_version", "mk64_items_asset", "mk64_items_audio_asset"}[missing]);
        { std::ofstream out(scratch / "catalogue.json"); out << legacy.dump(); }
        try {
            rr64::mk64_import::validate_pack(scratch, {});
            return 5;
        } catch (const std::runtime_error &error) {
            if (std::string(error.what()).find("reimport") == std::string::npos) return 6;
        }
    }
    std::cout << "Complete private c40 import validated; all three missing capability fields request reimport.\n"
              << identity << '\n';
}
