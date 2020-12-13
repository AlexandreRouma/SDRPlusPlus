#include <gui/style.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <config.h>

namespace style {
    ImFont* baseFont;
    ImFont* bigFont;
    ImFont* hugeFont;

    void setDefaultStyle() {
        ImGui::GetStyle().WindowRounding = 0.0f;
        ImGui::GetStyle().ChildRounding = 0.0f;
        ImGui::GetStyle().FrameRounding = 0.0f;
        ImGui::GetStyle().GrabRounding = 0.0f;
        ImGui::GetStyle().PopupRounding = 0.0f;
        ImGui::GetStyle().ScrollbarRounding = 0.0f;

        baseFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(((std::string)(ROOT_DIR "/res/fonts/Roboto-Medium.ttf")).c_str(), 16.0f);
        bigFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(((std::string)(ROOT_DIR "/res/fonts/Roboto-Medium.ttf")).c_str(), 42.0f);
        hugeFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(((std::string)(ROOT_DIR "/res/fonts/Roboto-Medium.ttf")).c_str(), 128.0f);

        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();
    }

    void testtt() {
        ImGui::StyleColorsLight();
    }

    void setDarkStyle() {
        ImGui::GetStyle().WindowRounding = 0.0f;
        ImGui::GetStyle().ChildRounding = 0.0f;
        ImGui::GetStyle().FrameRounding = 0.0f;
        ImGui::GetStyle().GrabRounding = 0.0f;
        ImGui::GetStyle().PopupRounding = 0.0f;
        ImGui::GetStyle().ScrollbarRounding = 0.0f;

        baseFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(((std::string)(ROOT_DIR "/res/fonts/Roboto-Medium.ttf")).c_str(), 16.0f);
        bigFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(((std::string)(ROOT_DIR "/res/fonts/Roboto-Medium.ttf")).c_str(), 42.0f);
        hugeFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(((std::string)(ROOT_DIR "/res/fonts/Roboto-Medium.ttf")).c_str(), 128.0f);

        ImGui::StyleColorsDark();

        auto& style = ImGui::GetStyle();

        ImVec4* colors = style.Colors;

        colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
        colors[ImGuiCol_ChildBg]                = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.44f, 0.44f, 0.44f, 0.40f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.44f, 0.44f, 0.44f, 0.45f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.44f, 0.44f, 0.44f, 0.40f);
        colors[ImGuiCol_Header]                 = ImVec4(0.63f, 0.63f, 0.70f, 0.31f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.63f, 0.63f, 0.70f, 0.40f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.63f, 0.63f, 0.70f, 0.31f);
        colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.72f, 0.72f, 0.72f, 0.78f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.4f, 0.9f, 1.0f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.73f, 0.60f, 0.15f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.87f, 0.87f, 0.87f, 0.35f);
        colors[ImGuiCol_ModalWindowDarkening]   = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    }

    void beginDisabled() {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.44f, 0.44f, 0.44f, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.21f, 0.22f, 0.30f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 0.65f));
    }

    void endDisabled() {
        ImGui::PopItemFlag();
        ImGui::PopStyleColor(3);
    }
}