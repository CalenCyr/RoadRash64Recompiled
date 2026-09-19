#include "rr64_sync_log.hpp"
#include <fstream>
#include <string>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    const std::filesystem::path path(argv[1]);
    if (std::filesystem::exists(path)) return 3;
    {
        rr64::sync_log::Writer writer(path);
        rr64::sync_log::Sample sample{};
        sample.riders[0].valid = 1;
        for (unsigned i=0;i<100000;++i) { sample.frame=i; writer.submit(sample); }
        writer.stop();
    }
    std::ifstream input(path);
    std::string line;
    unsigned rows=0;
    unsigned long long drops=0;
    bool end=false;
    std::getline(input,line);
    while (std::getline(input,line)) {
        if (line.starts_with("# end dropped=")) { drops=std::stoull(line.substr(14)); end=true; }
        else if (!line.starts_with('#')) ++rows;
    }
    if (!end || rows+drops!=100000) return 4;
    const auto size=std::filesystem::file_size(path);
    { rr64::sync_log::Writer duplicate(path); duplicate.stop(); }
    return std::filesystem::file_size(path)==size ? 0 : 5;
}
