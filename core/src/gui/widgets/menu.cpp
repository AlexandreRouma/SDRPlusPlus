#include <gui/widgets/menu.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

Menu::Menu() {

}

void Menu::registerEntry(std::string name, void (*drawHandler)(void* ctx), void* ctx, ModuleManager::Instance* inst) {
    MenuItem_t item;
    item.drawHandler = drawHandler;
    item.ctx = ctx;
    item.inst = inst;
    items[name] = item;
    if (!isInOrderList(name)) {
        MenuOption_t opt;
        opt.name = name;
        opt.open = true;
        order.push_back(opt);
    }
}

void Menu::removeEntry(std::string name) {
    items.erase(name);
}

bool Menu::draw(bool updateStates) {
    bool changed = false;
    float menuWidth = ImGui::GetContentRegionAvailWidth();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    for (MenuOption_t& opt : order) {
        if (items.find(opt.name) == items.end()) {
            continue;
        }
        MenuItem_t& item = items[opt.name];

        ImRect orginalRect = window->WorkRect;
        if (item.inst != NULL) {
            window->WorkRect = ImRect(orginalRect.Min, ImVec2(orginalRect.Max.x - ImGui::GetTextLineHeight() - 6, orginalRect.Max.y));
        }

        if (updateStates) { ImGui::SetNextItemOpen(opt.open); }
        if (ImGui::CollapsingHeader((opt.name + "##sdrpp_main_menu").c_str())) {
            if (item.inst != NULL) {
                window->WorkRect = orginalRect;
                ImVec2 pos = ImGui::GetCursorPos();
                ImGui::SetCursorPosX(pos.x + menuWidth - ImGui::GetTextLineHeight() - 6);
                ImGui::SetCursorPosY(pos.y - 10 - ImGui::GetTextLineHeight());
                bool enabled = item.inst->isEnabled();
                if (ImGui::Checkbox(("##_menu_checkbox_" + opt.name).c_str(), &enabled)) {
                    enabled ? item.inst->enable() : item.inst->disable();
                }
                ImGui::SetCursorPos(pos);
            }

            // Check if the state changed
            if (!opt.open && !updateStates) {
                opt.open = true;
                changed = true;
            }

            item.drawHandler(item.ctx);
            ImGui::Spacing();
        }
        else if (item.inst != NULL) {
            window->WorkRect = orginalRect;
            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::SetCursorPosX(pos.x + menuWidth - ImGui::GetTextLineHeight() - 6);
            ImGui::SetCursorPosY(pos.y - 10 - ImGui::GetTextLineHeight());
            bool enabled = item.inst->isEnabled();
            if (ImGui::Checkbox(("##_menu_checkbox_" + opt.name).c_str(), &enabled)) {
                enabled ? item.inst->enable() : item.inst->disable();
            }
            ImGui::SetCursorPos(pos);

            if (opt.open && !updateStates) {
                opt.open = false;
                changed = true;
            }
        }
        else if (opt.open && !updateStates) {
            opt.open = false;
            changed = true;
        }
    }
    return changed;
}

bool Menu::isInOrderList(std::string name) {
    for (MenuOption_t opt : order) {
        if (opt.name == name) {
            return true;
        }
    }
    return false;
}
