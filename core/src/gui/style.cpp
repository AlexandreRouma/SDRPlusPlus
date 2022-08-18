#include <gui/style.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <core.h>
#include <config.h>
#include <spdlog/spdlog.h>
#include <filesystem>

namespace style {
    ImFont* baseFont;
    ImFont* bigFont;
    ImFont* hugeFont;
    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    ImFontConfig fontConfig;

#ifndef __ANDROID__
    float uiScale = 1.0f;
#else
    float uiScale = 3.0f;
#endif

    struct Font {
        std::string path;
        float size;

        int oversample;
    };

    void from_json(const json& j, Font& f) {
        f.path = j["path"].get<std::string>();
        f.size = j.value("size", 16.0f);
        f.oversample = j.value("oversample", 2);
    }

    void to_json(json& j, const Font& t) {
        j["path"] = t.path;
        j["size"] = t.size;
        j["oversample"] = t.oversample;
    }

    bool loadFonts(std::string resDir) {
        if (!std::filesystem::is_directory(resDir)) {
            spdlog::error("Invalid resource directory: {0}", resDir);
            return false;
        }

        // Create font range
        ImFontAtlas* atlas = ImGui::GetIO().Fonts;
        builder.AddRanges(atlas->GetGlyphRangesChineseFull());
        builder.AddRanges(atlas->GetGlyphRangesCyrillic());
        builder.BuildRanges(&ranges);
  
        core::configManager.acquire();
        auto baseFontNames = core::configManager.conf["baseFonts"].get<std::vector<std::string>>();
        auto bigFontNames = core::configManager.conf["bigFonts"].get<std::vector<std::string>>();
        auto hugeFontNames = core::configManager.conf["hugeFonts"].get<std::vector<std::string>>();

        std::vector<Font> baseFonts, bigFonts, hugeFonts;

        for (auto &fontName : baseFontNames) {
            baseFonts.push_back(core::configManager.conf["fonts"][fontName].get<Font>());
        }

        for (auto &fontName : bigFontNames) {
            bigFonts.push_back(core::configManager.conf["fonts"][fontName].get<Font>());
        }

        for (auto &fontName : hugeFontNames) {
            hugeFonts.push_back(core::configManager.conf["fonts"][fontName].get<Font>());
        }

        // Build the baseFont
        fontConfig.MergeMode = false;
        for (auto &font : baseFonts) {
            fontConfig.OversampleH = font.oversample;
            
            baseFont = atlas->AddFontFromFileTTF(((std::string)(resDir + font.path)).c_str(), font.size * uiScale, &fontConfig, ranges.Data);
            fontConfig.MergeMode = true;
        }
        
        // Build the bigFont
        fontConfig.MergeMode = false;
        for (auto &font : bigFonts) {
            fontConfig.OversampleH = font.oversample;
    
            bigFont = atlas->AddFontFromFileTTF(((std::string)(resDir + font.path)).c_str(), 2.8125f * font.size * uiScale, &fontConfig, ranges.Data);
            fontConfig.MergeMode = true;
        }
        
        // Build the hugeFont
        fontConfig.MergeMode = false;
        for (auto &font : hugeFonts) {
            fontConfig.OversampleH = font.oversample;

            hugeFont = atlas->AddFontFromFileTTF(((std::string)(resDir + font.path)).c_str(), 8.0f * font.size * uiScale, &fontConfig, ranges.Data);
            fontConfig.MergeMode = true;
        }
        
        core::configManager.release();

        return true;
    }

    void beginDisabled() {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        auto& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;
        ImVec4 btnCol = colors[ImGuiCol_Button];
        ImVec4 frameCol = colors[ImGuiCol_FrameBg];
        ImVec4 textCol = colors[ImGuiCol_Text];
        btnCol.w = 0.15f;
        frameCol.w = 0.30f;
        textCol.w = 0.65f;
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameCol);
        ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    }

    void endDisabled() {
        ImGui::PopItemFlag();
        ImGui::PopStyleColor(3);
    }
}

namespace ImGui {
    void LeftLabel(const char* text) {
        float vpos = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(vpos + GImGui->Style.FramePadding.y);
        ImGui::TextUnformatted(text);
        ImGui::SameLine();
        ImGui::SetCursorPosY(vpos);
    }

    void FillWidth() {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    }
}
