#include "rr64_sync_log.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> fields(const std::string &line) {
    std::vector<std::string> result;
    std::istringstream input(line);
    std::string field;
    while(std::getline(input,field,','))result.push_back(field);
    return result;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    const std::filesystem::path path(argv[1]);
    if (std::filesystem::exists(path)) return 3;
    {
        rr64::sync_log::Writer writer(path);
        rr64::sync_log::Sample sample{};
        sample.riders[0].valid = 1;
        auto &d=sample.reconciliation;
        d.camera_view=2;
        d.camera_before={1,6,3,12,{1,2,3,4},{5,6,7}};
        d.camera_replayed={1,0,3,3,{8,9,10,11},{12,13,14}};
        d.camera_after={1,6,3,3,{15,16,17,18},{19,20,21}};
        d.camera_authority={1,3,{22,23,24,25}};
        for (unsigned i=0;i<100000;++i) { sample.frame=i; writer.submit(sample); }
        writer.stop();
    }
    std::ifstream input(path);
    std::string line;
    unsigned rows=0;
    unsigned long long drops=0;
    bool end=false;
    std::getline(input,line);
    const auto header=fields(line);
    std::vector<std::string> camera_names{"camera_view"};
    for(const auto *stage:{"before","replayed","after"})
        for(const auto *name:{"valid","mode","actor","flags","qx","qy","qz","qw","anchor_x","anchor_y","anchor_z"})
            camera_names.push_back(std::string("camera_")+stage+"_"+name);
    for(const auto *name:{"valid","flags","qx","qy","qz","qw"})
        camera_names.push_back(std::string("camera_authority_")+name);
    const std::vector<double> camera_values{2,
        1,6,3,12,1,2,3,4,5,6,7, 1,0,3,3,8,9,10,11,12,13,14,
        1,6,3,3,15,16,17,18,19,20,21, 1,3,22,23,24,25};
    if(header.size()!=104 || camera_names.size()!=40 || camera_values.size()!=40)return 6;
    for(unsigned i=0;i<40;++i)if(header[i+64]!=camera_names[i])return 7;
    while (std::getline(input,line)) {
        if (line.starts_with("# end dropped=")) { drops=std::stoull(line.substr(14)); end=true; }
        else if (!line.starts_with('#')) {
            const auto values=fields(line);
            if(values.size()!=header.size())return 8;
            for(unsigned i=0;i<40;++i)if(std::stod(values[i+64])!=camera_values[i])return 9;
            ++rows;
        }
    }
    if (!end || rows+drops!=100000) return 4;
    const auto size=std::filesystem::file_size(path);
    { rr64::sync_log::Writer duplicate(path); duplicate.stop(); }
    if(std::filesystem::file_size(path)!=size)return 5;
    std::cout<<"sync log smoke passed: 104 columns, 40 camera fields, "<<rows<<" rows + "<<drops<<" drops = 100000 samples\n";
    return 0;
}
