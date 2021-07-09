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

    bool menuClicked = false;
    std::string clickedMenuName = "";
    std::string draggedMenuName = "";
    std::vector<float> headerTops;
    int insertBefore = -1;
    std::string insertBeforeName = "";
    std::vector<std::string> displayedNames;

    int draggedId = 0;
    MenuOption_t draggedOpt;

    std::map<std::string, MenuItem_t> items;
};