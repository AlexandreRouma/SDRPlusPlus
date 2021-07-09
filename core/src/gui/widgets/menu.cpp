#include <gui/widgets/menu.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <gui/style.h>

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
    headerTops.clear();
    displayedNames.clear();
    float menuWidth = ImGui::GetContentRegionAvailWidth();
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    int id = 0;
    int rawId = 0;

    ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

    for (MenuOption_t& opt : order) {
        rawId++;
        if (items.find(opt.name) == items.end()) {
            continue;
        }
        if (opt.name == draggedMenuName) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", draggedMenuName.c_str());
            ImGui::EndTooltip();
            continue;
        }

        if (id == insertBefore && draggedMenuName != "") {
            if (updateStates) { ImGui::SetNextItemOpen(false); }
            ImVec2 posMin = ImGui::GetCursorScreenPos();
            ImVec2 posMax = ImVec2(posMin.x + menuWidth, posMin.y + ImGui::GetFrameHeight());
            style::beginDisabled();
            ImRect orignalRect = window->WorkRect;
            ImGui::CollapsingHeader((draggedMenuName + "##sdrpp_main_menu_dragging").c_str());
            if (items[draggedOpt.name].inst != NULL) {
                window->WorkRect = orignalRect;
                ImVec2 pos = ImGui::GetCursorPos();
                ImGui::SetCursorPosX(pos.x + menuWidth - ImGui::GetTextLineHeight() - 6);
                ImGui::SetCursorPosY(pos.y - 10 - ImGui::GetTextLineHeight());
                bool enabled = items[draggedOpt.name].inst->isEnabled();
                ImGui::Checkbox(("##_menu_checkbox_" + draggedOpt.name).c_str(), &enabled);
                ImGui::SetCursorPos(pos);
            }
            style::endDisabled();
            window->DrawList->AddRect(posMin, posMax, textColor);
        }
        id++;
        
        MenuItem_t& item = items[opt.name];
        

        ImRect orginalRect = window->WorkRect;
        if (item.inst != NULL) {
            window->WorkRect = ImRect(orginalRect.Min, ImVec2(orginalRect.Max.x - ImGui::GetTextLineHeight() - 6, orginalRect.Max.y));
        }

        ImVec2 posMin = ImGui::GetCursorScreenPos();
        ImVec2 posMax = ImVec2(posMin.x + menuWidth, posMin.y + ImGui::GetFrameHeight());

        headerTops.push_back(posMin.y);
        displayedNames.push_back(opt.name);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(posMin, posMax)) {
            menuClicked = true;
            clickedMenuName = opt.name;
        }

        if (menuClicked && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && draggedMenuName == "" && clickedMenuName == opt.name) {
            draggedMenuName = opt.name;
            draggedId = rawId-1;
            draggedOpt = opt;
            continue;
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

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && menuClicked) {
        
        if (draggedMenuName != "") {
            // Move menu
            order.erase(order.begin() + draggedId);

            if (insertBefore == headerTops.size()) {
                order.push_back(draggedOpt);
            }
            else if (insertBeforeName != "") {
                int beforeId = 0;
                for (int i = 0; i < order.size(); i++) {
                    if (order[i].name == insertBeforeName) {
                        beforeId = i;
                        break;
                    }
                }
                order.insert(order.begin() + beforeId, draggedOpt);
            }
            changed = true;
        }
        
        menuClicked = false;
        draggedMenuName = "";
        insertBeforeName = "";
        insertBefore = -1;
    }


    if (insertBefore == headerTops.size() && draggedMenuName != "") {
        if (updateStates) { ImGui::SetNextItemOpen(false); }
        ImVec2 posMin = ImGui::GetCursorScreenPos();
        ImVec2 posMax = ImVec2(posMin.x + menuWidth, posMin.y + ImGui::GetFrameHeight());
        style::beginDisabled();
        ImRect orignalRect = window->WorkRect;
        ImGui::CollapsingHeader((draggedMenuName + "##sdrpp_main_menu_dragging").c_str());
        if (items[draggedOpt.name].inst != NULL) {
            window->WorkRect = orignalRect;
            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::SetCursorPosX(pos.x + menuWidth - ImGui::GetTextLineHeight() - 6);
            ImGui::SetCursorPosY(pos.y - 10 - ImGui::GetTextLineHeight());
            bool enabled = items[draggedOpt.name].inst->isEnabled();
            ImGui::Checkbox(("##_menu_checkbox_" + draggedOpt.name).c_str(), &enabled);
            ImGui::SetCursorPos(pos);
        }
        style::endDisabled();
        window->DrawList->AddRect(posMin, posMax, textColor);
    }

    if (draggedMenuName != "") {
        insertBefore = headerTops.size();
        ImVec2 mPos = ImGui::GetMousePos();
        for (int i = 0; i < headerTops.size(); i++) {
            if (headerTops[i] > mPos.y) {
                insertBefore = i;
                insertBeforeName = displayedNames[i];
                break;
            }
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
