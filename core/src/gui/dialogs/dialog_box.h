#pragma once
#include "dialog_box.h"
#include <imgui.h>
#include <string>
#include <gui/gui.h>

#define GENERIC_DIALOG_BUTTONS_OK           "Ok\0"
#define GENERIC_DIALOG_BUTTONS_YES_NO       "Yes\0No\0"
#define GENERIC_DIALOG_BUTTONS_APPLY_CANCEL "Apply\0Cancel\0"
#define GENERIC_DIALOG_BUTTONS_OK_CANCEL    "Ok\0Cancel\0"

#define GENERIC_DIALOG_BUTTON_OK    0
#define GENERIC_DIALOG_BUTTON_YES   0
#define GENERIC_DIALOG_BUTTON_NO    1
#define GENERIC_DIALOG_BUTTON_APPLY 0
#define GENERIC_DIALOG_BUTTON_CANCE 1

namespace ImGui {
    template <typename Func>
    int GenericDialog(const char* id, bool& open, const char* buttons, Func draw) {
        // If not open, return
        if (!open) { return -1; }

        // Draw popup
        gui::mainWindow.lockWaterfallControls = true;
        std::string idstr = std::string("##") + std::string(id);
        ImGui::OpenPopup(id);
        if (ImGui::BeginPopup(id, ImGuiWindowFlags_NoResize)) {
            // Draw widgets
            draw();

            // Draw buttons
            int bid = 0;
            while (buttons[0]) {
                int len = strlen(buttons);

                // Draw button
                if (bid) { ImGui::SameLine(); }
                if (ImGui::Button((buttons + idstr).c_str())) {
                    open = false;
                    ImGui::EndPopup();
                    return bid;
                }

                buttons += len + 1;
                bid++;
            }

            ImGui::EndPopup();
        }

        return -1;
    }
}