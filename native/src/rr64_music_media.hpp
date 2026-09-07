#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
#include <chrono>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#endif

namespace rr64::music {
// Called only by the decode worker. No video stream, network URL or UI work.
inline std::vector<std::int16_t> decode_media(const std::filesystem::path& path,
    std::size_t maximum_frames) {
#ifdef _WIN32
    struct Runtime {
        HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        HRESULT mf=FAILED(com)?E_FAIL:MFStartup(MF_VERSION);
        ~Runtime(){if(SUCCEEDED(mf))MFShutdown();if(SUCCEEDED(com))CoUninitialize();}
    } runtime;
    if(FAILED(runtime.mf))return {};
    using Microsoft::WRL::ComPtr;
    ComPtr<IMFSourceReader> reader;
    ComPtr<IMFMediaType> format, actual;
    if(FAILED(MFCreateSourceReaderFromURL(path.c_str(),nullptr,&reader)) ||
        FAILED(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE)) ||
        FAILED(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM,TRUE)) ||
        FAILED(MFCreateMediaType(&format)) ||
        FAILED(format->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio)) ||
        FAILED(format->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_PCM)) ||
        FAILED(format->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,2)) ||
        FAILED(format->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,48000)) ||
        FAILED(format->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16)) ||
        FAILED(format->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,4)) ||
        FAILED(format->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,192000)) ||
        FAILED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,nullptr,format.Get())) ||
        FAILED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,&actual)))return {};
    const auto valid_format=[](IMFMediaType* type){
        GUID subtype{};
        return SUCCEEDED(type->GetGUID(MF_MT_SUBTYPE,&subtype)) && subtype==MFAudioFormat_PCM &&
            MFGetAttributeUINT32(type,MF_MT_AUDIO_NUM_CHANNELS,0)==2 &&
            MFGetAttributeUINT32(type,MF_MT_AUDIO_SAMPLES_PER_SECOND,0)==48000 &&
            MFGetAttributeUINT32(type,MF_MT_AUDIO_BITS_PER_SAMPLE,0)==16;
    };
    if(!valid_format(actual.Get()))return {};
    std::vector<std::int16_t> pcm;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    for(unsigned calls=0;calls<600000 && std::chrono::steady_clock::now()<deadline;++calls){
        DWORD flags=0; ComPtr<IMFSample> sample;
        if(FAILED(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,nullptr,&flags,nullptr,&sample)) ||
            (flags&MF_SOURCE_READERF_ERROR))return {};
        if(flags&MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED){
            actual.Reset();
            if(FAILED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,&actual)) ||
                !valid_format(actual.Get()))return {};
        }
        if(sample){
            ComPtr<IMFMediaBuffer> buffer;
            if(FAILED(sample->ConvertToContiguousBuffer(&buffer)))return {};
            BYTE* data=nullptr;DWORD length=0;
            if(FAILED(buffer->Lock(&data,nullptr,&length)))return {};
            struct Unlock {IMFMediaBuffer* b;~Unlock(){b->Unlock();}} unlock{buffer.Get()};
            if(length%4 || length/2>maximum_frames*2-pcm.size())return {};
            const auto old=pcm.size();pcm.resize(old+length/2);
            if(length)std::memcpy(pcm.data()+old,data,length);
        }
        if(flags&MF_SOURCE_READERF_ENDOFSTREAM)return pcm;
    }
#endif
    return {};
}
}
