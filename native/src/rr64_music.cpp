#include "rr64_music.hpp"
#include "rr64_native.hpp"
#include "rr64_music_media.hpp"
#ifndef RR64_MUSIC_HEADLESS
#include "librecomp/config.hpp"


#endif
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cctype>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <memory>
#include <vector>
#include <fstream>
#include "../lib/rt64/src/contrib/stb/stb_vorbis.c"
namespace rr64::music {
namespace {
struct Track { std::string title; std::vector<std::int16_t> pcm; };
std::vector<std::filesystem::path> files;
std::atomic_uint selection{0};
std::atomic<float> volume{0.65f};
std::atomic_bool playing{false};
std::atomic<std::shared_ptr<const Track>> current;
std::shared_ptr<const Track> retained;
std::future<std::shared_ptr<const Track>> loading;
unsigned requested=0, loading_id=0, installed=0;
bool was_race=false;


std::uint64_t race_number=0;
std::atomic_uint restart{0};

constexpr std::size_t max_frames=48000u*60u*15u;
bool supported_extension(std::string ext) {
    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
    return ext==".wav" || ext==".ogg" || ext==".flac" || ext==".mp3" ||
        ext==".mp4" || ext==".m4a" || ext==".aac" || ext==".wma";
}
std::shared_ptr<const Track> decode(std::filesystem::path path) {
    try {
        std::error_code ec;
        const auto bytes=std::filesystem::file_size(path,ec);
        if(ec||bytes>512u*1024u*1024u||!supported_extension(path.extension().string()))return {};
        auto result=std::make_shared<Track>();result->title=path.stem().string();
        SDL_AudioSpec spec{}; std::vector<std::int16_t> source;
        Uint8* wav=nullptr;Uint32 length=0;
        auto ext=path.extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(ext!=".wav" && ext!=".ogg") {
            result->pcm=decode_media(path,max_frames);
            return result->pcm.empty()?std::shared_ptr<const Track>{}:result;
        }
        if(bytes>128u*1024u*1024u)return {};
        if(ext==".ogg") {
            std::ifstream file(path,std::ios::binary);
            std::vector<unsigned char> data(static_cast<std::size_t>(bytes));
            if(!file.read(reinterpret_cast<char*>(data.data()),data.size()))return {};
            int error=0;auto* v=stb_vorbis_open_memory(data.data(),int(data.size()),&error,nullptr);
            if(!v)return {};
            const auto info=stb_vorbis_get_info(v);
            spec.freq=int(info.sample_rate);spec.channels=2;spec.format=AUDIO_S16SYS;
            std::array<short,8192> chunk{};
            for(;;){int n=stb_vorbis_get_samples_short_interleaved(v,2,chunk.data(),int(chunk.size()));if(!n)break;
                if(source.size()+std::size_t(n)*2>max_frames*2){stb_vorbis_close(v);return {};}
                source.insert(source.end(),chunk.begin(),chunk.begin()+n*2);
            }
            stb_vorbis_close(v);
        } else {
            auto* rw=SDL_RWFromFile(path.string().c_str(),"rb");
            if(!rw||!SDL_LoadWAV_RW(rw,1,&spec,&wav,&length))return {};
        }
        const auto* raw=wav?wav:reinterpret_cast<const Uint8*>(source.data());
        const auto raw_size=wav?std::size_t(length):source.size()*2;
        SDL_AudioCVT cvt{};
        if(spec.freq<8000||spec.freq>192000||spec.channels==0||
            SDL_BuildAudioCVT(&cvt,spec.format,spec.channels,spec.freq,AUDIO_S16SYS,2,48000)<0||
            raw_size>max_frames*4||raw_size*std::size_t(cvt.len_mult)>512u*1024u*1024u){if(wav)SDL_FreeWAV(wav);return {};}
        std::vector<Uint8> converted(raw_size*std::size_t(cvt.len_mult));
        std::copy(raw,raw+raw_size,converted.data());if(wav)SDL_FreeWAV(wav);
        cvt.buf=converted.data();cvt.len=int(raw_size);
        if(SDL_ConvertAudio(&cvt)<0||cvt.len_cvt<4||std::size_t(cvt.len_cvt)>max_frames*4)return {};
        result->pcm.resize(std::size_t(cvt.len_cvt)/2);
        std::memcpy(result->pcm.data(),converted.data(),result->pcm.size()*2);
        return result;
    }catch(...){return {};}
}

}
#ifndef RR64_MUSIC_HEADLESS
void configure(recomp::config::Config& config,const std::filesystem::path& directory){
    std::error_code ec;std::filesystem::create_directories(directory,ec);
    for(std::filesystem::directory_iterator it(directory,ec),end;!ec&&it!=end;it.increment(ec)){
        if(!it->is_regular_file(ec))continue;auto ext=it->path().extension().string();
        std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(supported_extension(ext)&&files.size()<256)files.push_back(it->path());
    }
    std::sort(files.begin(),files.end());
    std::vector<recomp::config::ConfigOptionEnumOption> options{{0u,"original","Original soundtrack"},{1u,"playlist","Custom playlist"}};
    for(unsigned i=0;i<files.size();++i)options.emplace_back(i+2,files[i].filename().string(),files[i].stem().string());
    config.add_enum_option("custom_music_track","Music Track","Add WAV, OGG, FLAC, MP3, MP4, M4A, AAC or WMA files to the music folder, then restart. Custom playlist rotates each race; MP4 uses audio only.",options,0u);
    config.add_option_change_callback("custom_music_track",[](auto value,auto,auto){selection.store(static_cast<unsigned>(std::get<std::uint32_t>(value)));});
    config.add_percent_number_option("custom_music_volume","Custom Music Volume","Volume of the custom soundtrack; game effects keep their own volume.",65.0);
    config.add_option_change_callback("custom_music_volume",[](auto value,auto,auto){volume.store(float(std::get<double>(value)/100.0));});
}
void update_ui(){
    const bool race=rr64_is_race_mode_active()!=0;
    const bool started=race&&!was_race;
    if(started){++race_number;restart.fetch_add(1);}
    was_race=race;
    const auto selected=selection.load();
    requested=selected==1?(files.empty()?0:unsigned((race&&race_number?race_number-1:race_number)%files.size())+2):selected;
    if(requested>=files.size()+2)requested=0;
    
    if(loading.valid()&&loading.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto decoded=loading.get();
        if(loading_id==requested){retained=decoded;current.store(decoded);installed=loading_id;}
    }
    if(!requested){if(installed){current.store({});retained.reset();installed=0;}}
    else if(requested!=installed&&!loading.valid()){
        current.store({});loading_id=requested;
        loading=std::async(std::launch::async,decode,files[requested-2]);
    }
    playing.store(race&&requested==installed&&bool(retained));
}
#endif
void mix(std::span<std::int16_t> output,std::uint32_t rate){
    static std::shared_ptr<const Track> track;static double cursor=0;static unsigned epoch=0;
    auto next=current.load();const auto now=restart.load();if(track!=next||epoch!=now){track=next;cursor=0;epoch=now;}
    if(!playing.load()||!track||!rate)return;
    const auto frames=track->pcm.size()/2;if(!frames)return;const float gain=volume.load();
    for(std::size_t i=0;i+1<output.size();i+=2){auto a=std::size_t(cursor)%frames,b=(a+1)%frames;float f=float(cursor-std::floor(cursor));
        for(unsigned ch=0;ch<2;++ch){float sample=track->pcm[a*2+ch]*(1-f)+track->pcm[b*2+ch]*f;
            output[i+ch]=std::int16_t(std::clamp(float(output[i+ch])+sample*gain,-32768.0f,32767.0f));}
        cursor+=48000.0/double(rate);if(cursor>=frames)cursor=std::fmod(cursor,double(frames));
    }
}
}
extern "C" unsigned int rr64_music_volume_update(unsigned int original){
    static bool muted=false;const bool now=rr64::music::playing.load();const bool changed=now!=muted;muted=now;return (now||changed)?1u:original;
}
extern "C" unsigned int rr64_music_stock_volume(unsigned int original){return rr64::music::playing.load()?0u:original;}
