#pragma once
#include <map>
#include <string>
#include <mutex>
#include <spdlog/spdlog.h>

struct ModuleComInterface {
    std::string moduleName;
    void* ctx;
    void (*handler)(int code, void* in, void* out, void* ctx);
};

class ModuleComManager {
public:
    bool registerInterface(std::string moduleName, std::string name, void (*handler)(int code, void* in, void* out, void* ctx), void* ctx);
    bool unregisterInterface(std::string name);
    bool interfaceExists(std::string name);
    std::string getModuleName(std::string name);
    bool callInterface(std::string name, int code, void* in, void* out);

private:
    std::mutex mtx;
    std::map<std::string, ModuleComInterface> interfaces;
};