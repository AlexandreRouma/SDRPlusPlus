#pragma once
#include <string>
#include <vector>
#include <map>

class Menu {
public:
    Menu();

    struct MenuItem_t {
        void (*drawHandler)(void* ctx);
        void* ctx;
    };

    void registerEntry(std::string name, void (*drawHandler)(void* ctx), void* ctx = NULL);
    void removeEntry(std::string name);
    void draw();

    std::vector<std::string> order;

private:
    std::map<std::string, MenuItem_t> items;
};