#pragma once
#include <string>
#include <vector>
#include <map>
#include <module.h>

class Menu {
public:
    Menu();

    struct MenuOption_t {
        std::string name;
        bool open;
    };

    struct MenuItem_t {
        void (*drawHandler)(void* ctx);
        void* ctx;
        ModuleManager::Instance* inst;
    };

    void registerEntry(std::string name, void (*drawHandler)(void* ctx), void* ctx = NULL, ModuleManager::Instance* inst = NULL);
    void removeEntry(std::string name);
    bool draw(bool updateStates);

    std::vector<MenuOption_t> order;

private:
    bool isInOrderList(std::string name);

    std::map<std::string, MenuItem_t> items;
};