#include <gui/menus/vfo_color.h>
#include <gui/gui.h>
#include <gui/widgets/waterfall.h>
#include <signal_path/signal_path.h>
#include <string>
#include <core.h>
#include <map>

namespace vfo_color_menu {
    std::map<std::string, ImVec4> vfoColors;
    std::string openName = "";
    EventHandler<VFOManager::VFO*> vfoAddHndl;

    void vfoAddHandler(VFOManager::VFO* vfo, void* ctx) {
        std::string name = vfo->getName();
        if (vfoColors.find(name) != vfoColors.end()) {
            ImVec4 col = vfoColors[name];
            vfo->setColor(IM_COL32((int)roundf(col.x * 255), (int)roundf(col.y * 255), (int)roundf(col.z * 255), 50));
            return;
        }
        vfo->setColor(IM_COL32(255, 255, 255, 50));
        vfoColors[name] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void init() {
        // Load colors from config
        bool modified = false;
        core::configManager.acquire();
        json conf = core::configManager.conf["vfoColors"];
        for (auto& [name, val] : conf.items()) {
            // If not a string, repair with default
            if (!val.is_string()) { 
                core::configManager.conf["vfoColors"][name] = "#FFFFFF";
                vfoColors[name] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                modified = true;
                if (sigpath::vfoManager.vfoExists(name)) {
                    sigpath::vfoManager.setColor(name, IM_COL32(255, 255, 255, 50));
                }
                continue;
            }

            // If not a valid hex color, repair with default
            std::string col = val;
            if (col[0] != '#' || !std::all_of(col.begin() + 1, col.end(), ::isxdigit)) {
                core::configManager.conf["vfoColors"][name] = "#FFFFFF";
                vfoColors[name] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                modified = true;
                if (sigpath::vfoManager.vfoExists(name)) {
                    sigpath::vfoManager.setColor(name, IM_COL32(255, 255, 255, 50));
                }
                continue;
            }

            // Since the color is valid, decode it and set the vfo's color
            float r, g, b;
            r = std::stoi(col.substr(1, 2), NULL, 16);
            g = std::stoi(col.substr(3, 2), NULL, 16);
            b = std::stoi(col.substr(5, 2), NULL, 16);
            vfoColors[name] = ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
            if (sigpath::vfoManager.vfoExists(name)) {
                sigpath::vfoManager.setColor(name, IM_COL32((int)roundf(r), (int)roundf(g), (int)roundf(b), 50));
            }
        }

        // Iterate existing VFOs and set their color if in the config, if not set to default
        for (auto& [name, vfo] : gui::waterfall.vfos) {
            if (vfoColors.find(name) == vfoColors.end()) {
                vfoColors[name] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                vfo->color = IM_COL32(255, 255, 255, 50);
                modified = true;
            }
        }

        vfoAddHndl.handler = vfoAddHandler;
        sigpath::vfoManager.vfoCreatedEvent.bindHandler(&vfoAddHndl);
        core::configManager.release(modified);
    }

    void draw(void* ctx) {
        ImGui::BeginTable("VFO Color Buttons Table", 2);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::Button("Auto Color##vfo_color", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
            float delta = 1.0f / (float)gui::waterfall.vfos.size();
            float hue = 0;
            for (auto& [name, vfo] : gui::waterfall.vfos) {
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(hue, 0.5f, 1.0f, r, g, b);
                vfoColors[name] = ImVec4(r, g, b, 1.0f);
                vfo->color = IM_COL32((int)roundf(r * 255), (int)roundf(g * 255), (int)roundf(b * 255), 50);
                hue += delta;
                core::configManager.acquire();
                char buf[16];
                sprintf(buf, "#%02X%02X%02X", (int)roundf(r * 255), (int)roundf(g * 255), (int)roundf(b * 255));
                core::configManager.conf["vfoColors"][name] = buf;
                core::configManager.release(true);
            }
        }

        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button("Clear All##vfo_color", ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
            for (auto& [name, vfo] : gui::waterfall.vfos) {
                vfoColors[name] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                vfo->color = IM_COL32(255, 255, 255, 50);
                core::configManager.acquire();
                char buf[16];
                core::configManager.conf["vfoColors"][name] = "#FFFFFF";
                core::configManager.release(true);
            }
        }

        ImGui::EndTable();

        ImGui::BeginTable("VFO Color table", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders);
        for (auto& [name, vfo] : gui::waterfall.vfos) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImVec4 col(1.0f, 1.0f, 1.0f, 1.0f);
            if (vfoColors.find(name) != vfoColors.end()) {
                col = vfoColors[name];
            }
            if (ImGui::ColorEdit3(("##vfo_color_"+name).c_str(), (float*)&col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                vfoColors[name] = col;
                vfo->color = IM_COL32((int)roundf(col.x * 255), (int)roundf(col.y * 255), (int)roundf(col.z * 255), 50);
                core::configManager.acquire();
                char buf[16];
                sprintf(buf, "#%02X%02X%02X", (int)roundf(col.x * 255), (int)roundf(col.y * 255), (int)roundf(col.z * 255));
                core::configManager.conf["vfoColors"][name] = buf;
                core::configManager.release(true);
            }
            ImGui::SameLine();
            ImGui::Text(name.c_str());
        }
        ImGui::EndTable();
    }
}