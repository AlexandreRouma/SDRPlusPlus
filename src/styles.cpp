#include <style.h>

namespace style {
    void setDefaultStyle() {
        ImGui::GetStyle().WindowRounding = 0.0f;
        ImGui::GetStyle().ChildRounding = 0.0f;
        ImGui::GetStyle().FrameRounding = 0.0f;
        ImGui::GetStyle().GrabRounding = 0.0f;
        ImGui::GetStyle().PopupRounding = 0.0f;
        ImGui::GetStyle().ScrollbarRounding = 0.0f;

        ImGui::GetIO().Fonts->AddFontFromFileTTF("res/fonts/Roboto-Medium.ttf", 16.0f);

        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();
    }

    void beginDisabled() {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35f, 0.35f, 0.35f, 0.35f));
    }

    void endDisabled() {
        ImGui::PopItemFlag();
        ImGui::PopStyleColor(2);
    }
}