#pragma once
#include <string>
#include <map>
#include <filesystem>
#include <stdint.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <gui/waterfall.h>
#include <json.hpp>

#ifdef _WIN32
#ifdef SDRPP_IS_CORE
#define SDRPP_EXPORT extern "C" __declspec(dllexport)
#else
#define SDRPP_EXPORT extern "C" __declspec(dllimport)
#endif
#else
#define SDRPP_EXPORT extern
#endif

#ifdef _WIN32
#include <Windows.h>
#define MOD_EXPORT extern "C" __declspec(dllexport)
#else
#include <dlfcn.h>
#define MOD_EXPORT extern "C"
#endif

namespace mod {

    struct Module_t {
#ifdef _WIN32
        HINSTANCE inst;
#else
        void* inst;
#endif
        void (*_INIT_)();
        void* (*_CREATE_INSTANCE)(std::string);
        void (*_DELETE_INSTANCE)();
        void (*_STOP_)();
        void* ctx;
    };

    struct ModuleInfo_t {
        char* name;
        char* description;
        char* author;
        char* version;
    };

    void initAPI(ImGui::WaterFall* wtf);
    void loadModule(std::string path, std::string name);
    void broadcastEvent(int eventId);
    void loadFromList(std::string path);
    
    extern std::map<std::string, Module_t> modules;
    extern std::vector<std::string> moduleNames;
};

#define MOD_INFO    MOD_EXPORT const mod::ModuleInfo_t _INFO