#pragma once
#include <string>
#include <vector>
#include <map>
#include <new_module.h>

class Menu {
public:
    Menu();

    struct MenuItem_t {
        void (*drawHandler)(void* ctx);
        void* ctx;
        ModuleManager::Instance* inst;
    };

    void registerEntry(std::string name, void (*drawHandler)(void* ctx), void* ctx = NULL, ModuleManager::Instance* inst = NULL);
    void removeEntry(std::string name);
    void draw();

    std::vector<std::string> order;

private:
    bool isInOrderList(std::string name);

    std::map<std::string, MenuItem_t> items;
};