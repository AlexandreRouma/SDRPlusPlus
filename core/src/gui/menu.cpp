#include <gui/menu.h>

Menu::Menu() {

}

void Menu::registerEntry(std::string name, void (*drawHandler)(void* ctx), void* ctx) {
    MenuItem_t item;
    item.drawHandler = drawHandler;
    item.ctx = ctx;
    items[name] = item;
}

void Menu::removeEntry(std::string name) {
    items.erase(name);
}

void Menu::draw() {
    MenuItem_t item;
    for (std::string name : order) {
        item = items[name];
        item.drawHandler(item.ctx);
    }
}
