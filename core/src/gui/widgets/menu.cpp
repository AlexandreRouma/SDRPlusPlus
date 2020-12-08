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
        order.push_back(name);
    }
}

void Menu::removeEntry(std::string name) {
    items.erase(name);
}

void Menu::draw() {
    MenuItem_t item;
    float menuWidth = ImGui::GetContentRegionAvailWidth();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    for (std::string name : order) {
        if (items.find(name) == items.end()) {
            continue;
        }
        item = items[name];

        ImRect orginalRect = window->WorkRect;
        if (item.inst != NULL) {
            
            window->WorkRect = ImRect(orginalRect.Min, ImVec2(orginalRect.Max.x - ImGui::GetTextLineHeight() - 6, orginalRect.Max.y));
        }

        if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (item.inst != NULL) {
                window->WorkRect = orginalRect;
                ImVec2 pos = ImGui::GetCursorPos();
                ImGui::SetCursorPosX(pos.x + menuWidth - ImGui::GetTextLineHeight() - 6);
                ImGui::SetCursorPosY(pos.y - 10 - ImGui::GetTextLineHeight());
                bool enabled = item.inst->isEnabled();
                if (ImGui::Checkbox(("##_menu_checkbox_" + name).c_str(), &enabled)) {
                    enabled ? item.inst->enable() : item.inst->disable();
                }
                ImGui::SetCursorPos(pos);
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
            if (ImGui::Checkbox(("##_menu_checkbox_" + name).c_str(), &enabled)) {
                enabled ? item.inst->enable() : item.inst->disable();
            }
            ImGui::SetCursorPos(pos);
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
