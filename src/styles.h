#pragma once
#include <imgui.h>

void setImguiStyle(ImGuiIO& io) {
    ImGui::GetStyle().WindowRounding = 0.0f;
    ImGui::GetStyle().ChildRounding = 0.0f;
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding = 0.0f;
    ImGui::GetStyle().PopupRounding = 0.0f;
    ImGui::GetStyle().ScrollbarRounding = 0.0f;

    io.Fonts->AddFontFromFileTTF("res/fonts/Roboto-Medium.ttf", 16.0f);

    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
}