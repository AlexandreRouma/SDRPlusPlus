#include <gui/menu.h>

Menu::Menu() {

}

void Menu::registerEntry(std::string name, void (*drawHandler)(void* ctx), void* ctx) {
    MenuItem_t item;
    item.drawHandler = drawHandler;
    item.ctx = ctx;
    items[name] = item;
    if (!isInOrderList(name)) {
        order.push_back(name);
    }
}

void Menu::removeEntry(std::string name) {
    items.erase(name);
}

void Menu::draw() {
    MenuItem_t item;
    for (std::string name : order) {
        if (items.find(name) == items.end()) {
            continue;
        }
        item = items[name];
        if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            item.drawHandler(item.ctx);
            ImGui::Spacing();
        }
    }
}

bool Menu::isInOrderList(std::string name) {
    for (std::string _name : order) {
        if (_name == name) {
            return true;
        }
    }
    return false;
}
