#pragma once
#include <string>
#include <map>
#include <filesystem>
#include <stdint.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <Windows.h>
#define MOD_EXPORT  extern "C" \
                    __declspec(dllexport)
#else
#define MOD_EXPORT  extern "C"
#endif

namespace mod {
    struct API_t {

    };

    struct Module_t {
#ifdef _WIN32
        HINSTANCE inst;
#else
        void* inst;
#endif
        void* (*_INIT_)(API_t*, ImGuiContext*, std::string);
        void (*_DRAW_MENU_)(void*);
        void (*_STOP_)(void*);
        void* ctx;
    };

    void loadModule(std::string path, std::string name);
    
    extern std::map<std::string, Module_t> modules;
    extern std::vector<std::string> moduleNames;
};