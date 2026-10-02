#pragma once
#include "rr64_rider_skins.hpp"
inline rr64::rider_skins::Appearance skin_fixture(std::string id,std::string name,unsigned donor) {
    rr64::rider_skins::Appearance a{std::move(id),std::move(name),donor,{}};
    for(unsigned slot=0;slot<4;++slot){
        auto& t=a.textures[slot];t.width=slot==3?32:64;t.height=slot==3?16:32;
        t.bytes.resize(t.width*t.height+512);
        for(unsigned p=t.width*t.height+1;p<t.bytes.size();p+=2)t.bytes[p]=1;
    }
    return a;
}
