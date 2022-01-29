#include <gui/dialogs/loading_screen.h>
#include <gui/main_window.h>
#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <gui/icons.h>
#include <gui/style.h>
#include <credits.h>
#include <gui/gui.h>
#include <backend.h>

namespace LoadingScreen {

    void show(std::string msg) {
        backend::beginFrame();

        ImGui::Begin("Main", NULL, WINDOW_FLAGS);


        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
        ImGui::OpenPopup("Credits");
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginPopupModal("Credits", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

        ImGui::PushFont(style::hugeFont);
        ImGui::TextUnformatted("SDR++    ");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::Image(icons::LOGO, ImVec2(128, 128));

        ImVec2 origPos = ImGui::GetCursorPos();
        ImGui::SetCursorPosY(origPos.y + 50);
        ImGui::Text("%s", msg.c_str());
        ImGui::SetCursorPos(origPos);

        ImGui::EndPopup();
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(1);

        ImGui::End();

        backend::render(false);
    }
}