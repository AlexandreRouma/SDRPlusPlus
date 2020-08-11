#pragma once
#include <string>
#include <map>
#include <filesystem>
#include <stdint.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <dsp/types.h>
#include <dsp/stream.h>

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

        enum {
            REF_LOWER,
            REF_CENTER,
            REF_UPPER,
            _REF_COUNT
        };
    };

    enum {
        EVENT_STREAM_PARAM_CHANGED,
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
        void (*_HANDLE_EVENT_)(void*, int);
        void (*_STOP_)(void*);
        void* ctx;
    };

    void initAPI();
    void loadModule(std::string path, std::string name);
    void broadcastEvent(int eventId);
    
    extern std::map<std::string, Module_t> modules;
    extern std::vector<std::string> moduleNames;
};

extern mod::API_t* API;