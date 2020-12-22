#pragma once
#include <string>
#include <map>
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
#define SDRPP_MOD_EXTENTSION    ".dll"
#else
#include <dlfcn.h>
#define MOD_EXPORT extern "C"
#define SDRPP_MOD_EXTENTSION    ".so"
#endif

class ModuleManager {
public:
    struct ModuleInfo_t {
        const char* name;
        const char* description;
        const char* author;
        const int versionMajor;
        const int versionMinor;
        const int versionBuild;
        const int maxInstances;
    };

    class Instance {
    public:
        virtual void enable() = 0;
        virtual void disable() = 0;
        virtual bool isEnabled() = 0;
    };

    struct Module_t {
#ifdef _WIN32
        HMODULE handle;
#else
        void* handle;
#endif
        ModuleManager::ModuleInfo_t* info;
        void (*init)();
        ModuleManager::Instance* (*createInstance)(std::string name);
        void (*deleteInstance)(ModuleManager::Instance* instance);
        void (*end)();

        friend bool operator==(const Module_t& a, const Module_t& b) {
            if (a.handle != b.handle) { return false; }
            if (a.info != b.info) { return false; }
            if (a.init != b.init) { return false; }
            if (a.createInstance != b.createInstance) { return false; }
            if (a.deleteInstance != b.deleteInstance) { return false; }
            if (a.end != b.end) { return false; }
            return true;
        }
    };

    struct Instance_t {
        ModuleManager::Module_t module;
        ModuleManager::Instance* instance;
    };

    ModuleManager::Module_t loadModule(std::string path);

    void createInstance(std::string name, std::string module);
    void deleteInstance(std::string name);
    void deleteInstance(ModuleManager::Instance* instance);

    void enableInstance(std::string name);
    void disableInstance(std::string name);
    bool instanceEnabled(std::string name);

    int countModuleInstances(std::string module);

private:
    std::map<std::string, ModuleManager::Module_t> modules;
    std::map<std::string, ModuleManager::Instance_t> instances;

};

#define SDRPP_MOD_INFO    MOD_EXPORT const ModuleManager::ModuleInfo_t _INFO_