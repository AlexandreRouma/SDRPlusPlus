#include <gui/dialogs/credits.h>
#include <imgui.h>
#include <gui/icons.h>
#include <gui/style.h>
#include <config.h>
#include <credits.h>
#include <version.h>

namespace credits {
    ImFont* bigFont;

    void init() {
                
    }

    void show() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
        ImVec2 dispSize = ImGui::GetIO().DisplaySize;
        ImVec2 center = ImVec2(dispSize.x/2.0f, dispSize.y/2.0f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Credits");
        ImGui::BeginPopupModal("Credits", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

        ImGui::PushFont(style::hugeFont);
        ImGui::Text("SDR++          ");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::Image(icons::LOGO, ImVec2(128, 128));
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::Text("This software is brought to you by Alexandre Rouma with the help of\n\n");

        ImGui::Columns(4, "CreditColumns", true);

        ImGui::Text("Contributors");
        for (int i = 0; i < sdrpp_credits::contributorCount; i++) {
            ImGui::BulletText("%s", sdrpp_credits::contributors[i]);
        }

        ImGui::NextColumn();
        ImGui::Text("Libraries");
        for (int i = 0; i < sdrpp_credits::libraryCount; i++) {
            ImGui::BulletText("%s", sdrpp_credits::libraries[i]);
        }

        ImGui::NextColumn();
        ImGui::Text("Hardware Donators");
        for (int i = 0; i < sdrpp_credits::hardwareDonatorCount; i++) {
            ImGui::BulletText("%s", sdrpp_credits::hardwareDonators[i]);
        }

        ImGui::NextColumn();
        ImGui::Text("Patrons");
        for (int i = 0; i < sdrpp_credits::patronCount; i++) {
            ImGui::BulletText("%s", sdrpp_credits::patrons[i]);
        }

        ImGui::Columns(1, "CreditColumnsEnd", true);

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text("SDR++ v" VERSION_STR " (Built at " __TIME__ ", " __DATE__ ")");

        ImGui::EndPopup();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
}