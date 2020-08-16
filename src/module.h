#pragma once
#include <string>
#include <map>
#include <filesystem>
#include <stdint.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <waterfall.h>
#include <json.hpp>

#ifdef _WIN32
#include <Windows.h>
#define MOD_EXPORT  extern "C" \
                    __declspec(dllexport)
#else
#include <dlfcn.h>
#define MOD_EXPORT  extern "C"
#endif

namespace mod {
    struct API_t {
        dsp::stream<dsp::complex_t>* (*registerVFO)(std::string name, int reference, float offset, float bandwidth, float sampleRate, int blockSize);
        void (*setVFOOffset)(std::string name, float offset);
        void (*setVFOCenterOffset)(std::string name, float offset);
        void (*setVFOBandwidth)(std::string name, float bandwidth);
        void (*setVFOSampleRate)(std::string name, float sampleRate, float bandwidth);
        int (*getVFOOutputBlockSize)(std::string name);
        void (*setVFOReference)(std::string name, int ref);
        void (*removeVFO)(std::string name);
        
        std::string (*getSelectedVFOName)(void);
        void (*bindVolumeVariable)(float* vol);
        void (*unbindVolumeVariable)(void);

        float (*registerMonoStream)(dsp::stream<float>* stream, std::string name, std::string vfoName, int (*sampleRateChangeHandler)(void* ctx, float sampleRate), void* ctx);
        float (*registerStereoStream)(dsp::stream<dsp::StereoFloat_t>* stream, std::string name, std::string vfoName, int (*sampleRateChangeHandler)(void* ctx, float sampleRate), void* ctx);
        void (*startStream)(std::string name);
        void (*stopStream)(std::string name);
        void (*removeStream)(std::string name);
        dsp::stream<float>* (*bindToStreamMono)(std::string name, void (*streamRemovedHandler)(void* ctx), void (*sampleRateChangeHandler)(void* ctx, float sampleRate, int blockSize), void* ctx);
        dsp::stream<dsp::StereoFloat_t>* (*bindToStreamStereo)(std::string name, void (*streamRemovedHandler)(void* ctx), void (*sampleRateChangeHandler)(void* ctx, float sampleRate, int blockSize), void* ctx);
        void (*setBlockSize)(std::string name, int blockSize);
        void (*unbindFromStreamMono)(std::string name, dsp::stream<float>* stream);
        void (*unbindFromStreamStereo)(std::string name, dsp::stream<dsp::StereoFloat_t>* stream);

        enum {
            REF_LOWER,
            REF_CENTER,
            REF_UPPER,
            _REF_COUNT
        };
    };

    enum {
        EVENT_STREAM_PARAM_CHANGED,
        EVENT_SELECTED_VFO_CHANGED,
        _EVENT_COUNT
    };

    struct Module_t {
#ifdef _WIN32
        HINSTANCE inst;
#else
        void* inst;
#endif
        void* (*_INIT_)(API_t*, ImGuiContext*, std::string);
        void (*_DRAW_MENU_)(void*);
        void (*_NEW_FRAME_)(void*);
        void (*_HANDLE_EVENT_)(void*, int);
        void (*_STOP_)(void*);
        void* ctx;
    };

    void initAPI(ImGui::WaterFall* wtf);
    void loadModule(std::string path, std::string name);
    void broadcastEvent(int eventId);
    void loadFromList(std::string path);
    
    extern std::map<std::string, Module_t> modules;
    extern std::vector<std::string> moduleNames;
};

extern mod::API_t* API;