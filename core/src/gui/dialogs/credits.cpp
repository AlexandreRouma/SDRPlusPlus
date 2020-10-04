#include <gui/dialogs/credits.h>
#include <imgui.h>
#include <gui/icons.h>

namespace credits {
    ImFont* bigFont;

    void init() {
        // TODO: Have a font manager instead
        bigFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(((std::string)(ROOT_DIR "/res/fonts/Roboto-Medium.ttf")).c_str(), 128.0f);
    }

    void show() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
        ImGui::OpenPopup("Credits");
        ImGui::BeginPopupModal("Credits", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

        ImGui::PushFont(bigFont);
        ImGui::Text("SDR++    ");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::Image(icons::LOGO, ImVec2(128, 128));
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::Text("This software is brought to you by\n\n");

        ImGui::Columns(3, "CreditColumns", true);

        // Contributors
        ImGui::Text("Contributors");
        ImGui::BulletText("Ryzerth (Creator)");
        ImGui::BulletText("aosync");
        ImGui::BulletText("Benjamin Kyd");
        ImGui::BulletText("Tobias Mädel");
        ImGui::BulletText("Raov");
        ImGui::BulletText("Howard0su");

        // Libraries
        ImGui::NextColumn();
        ImGui::Text("Libraries");
        ImGui::BulletText("SoapySDR (PothosWare)");
        ImGui::BulletText("Dear ImGui (ocornut)");
        ImGui::BulletText("spdlog (gabime)");
        ImGui::BulletText("json (nlohmann)");
        ImGui::BulletText("portaudio (PA Comm.)");

        // Patrons
        ImGui::NextColumn();
        ImGui::Text("Patrons");
        ImGui::BulletText("SignalsEverywhere");
        ImGui::BulletText("Lee Donaghy");

        ImGui::EndPopup();
        ImGui::PopStyleVar(1);
    }
}