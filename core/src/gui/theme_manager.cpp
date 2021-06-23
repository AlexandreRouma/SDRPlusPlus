#include <json.hpp>
#include <gui/theme_manager.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

bool ThemeManager::loadThemesFromDir(std::string path) {
    // TEST JUST TO DUMP THE ORIGINAL THEME
    auto& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    printf("\n\n");
    for (auto [name, id] : IMGUI_COL_IDS) {
        ImVec4 col = colors[id];
        uint8_t r = 255 - (col.x * 255.0f);
        uint8_t g = 255 - (col.y * 255.0f);
        uint8_t b = 255 - (col.z * 255.0f);
        uint8_t a = col.w * 255.0f;
        printf("\"%s\": \"#%02X%02X%02X%02X\",\n", name.c_str(), r, g, b, a);
    }
    printf("\n\n");


    if (!std::filesystem::is_directory(path)) {
        spdlog::error("Theme directory doesn't exist: {0}", path);
        return false;
    }
    themes.clear();
    for (const auto & file : std::filesystem::directory_iterator(path)) {
        std::string _path = file.path().generic_string();
        if (file.path().extension().generic_string() != ".json") {
            continue;
        }
        loadTheme(_path);
    }
    return true;
}

bool ThemeManager::loadTheme(std::string path) {
    if (!std::filesystem::is_regular_file(path)) {
        spdlog::error("Theme file doesn't exist: {0}", path);
        return false;
    }

    // Load defaults in theme
    Theme thm;
    thm.author = "--";

    // Load JSON
    std::ifstream file(path.c_str());
    json data;
    file >> data;
    file.close();

    // Load theme name
    if (!data.contains("name")) {
        spdlog::error("Theme {0} is missing the name parameter", path);
        return false;
    }
    if (!data["name"].is_string()) {
        spdlog::error("Theme {0} contains invalid name field. Expected string", path);
        return false;
    }
    std::string name = data["name"];

    if (themes.find(name) != themes.end()) {
        spdlog::error("A theme named '{0}' already exists", name);
        return false;
    }

    // Load theme author if available
    if (data.contains("author")) {
        if (!data["author"].is_string()) {
            spdlog::error("Theme {0} contains invalid author field. Expected string", path);
            return false;
        }
        thm.author = data["author"];
    }

    // Iterate through all parameters and check their contents
    std::map<std::string, std::string> params = data;
    for (auto const& [param, val] : params) {
        if (param == "name" || param == "author") { continue; }

        // Exception for non-imgu colors
        if (param == "WaterfallBackground" || param == "ClearColor") {
            if (val[0] != '#' || !std::all_of(val.begin() + 1, val.end(), ::isxdigit) || val.length() != 9) {
                spdlog::error("Theme {0} contains invalid {1} field. Expected hex RGBA color", path, param);
                return false;
            }
            continue;
        }

        bool isValid = false;

        // If param is a color, check that it's a valid RGBA hex value
        if (IMGUI_COL_IDS.find(param) != IMGUI_COL_IDS.end()) {
            if (val[0] != '#' || !std::all_of(val.begin() + 1, val.end(), ::isxdigit) || val.length() != 9) {
                spdlog::error("Theme {0} contains invalid {1} field. Expected hex RGBA color", path, param);
                return false;
            }
            isValid = true;
        }

        if (!isValid) {
            spdlog::error("Theme {0} contains unknown {1} field.", path, param);
            return false;
        }
    }
    
    thm.data = data;
    themes[name] = thm;

    return true;
}

bool ThemeManager::applyTheme(std::string name) {
    if (themes.find(name) == themes.end()) {
        spdlog::error("Unknown theme: {0}", name);
        return false;
    }

    ImGui::StyleColorsDark();

    auto& style = ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;

    ImVec4* colors = style.Colors;
    Theme thm = themes[name];
    
    uint8_t ret[4];
    std::map<std::string, std::string> params = thm.data;
    for (auto const& [param, val] : params) {
        if (param == "name" || param == "author") { continue; }

        if (param == "WaterfallBackground") {
            decodeRGBA(val, ret);
            waterfallBg = ImVec4((float)ret[0]/255.0f, (float)ret[1]/255.0f, (float)ret[2]/255.0f, (float)ret[3]/255.0f);
            continue;
        }

        if (param == "ClearColor") {
            decodeRGBA(val, ret);
            clearColor = ImVec4((float)ret[0]/255.0f, (float)ret[1]/255.0f, (float)ret[2]/255.0f, (float)ret[3]/255.0f);
            continue;
        }

        // If param is a color, check that it's a valid RGBA hex value
        if (IMGUI_COL_IDS.find(param) != IMGUI_COL_IDS.end()) {
            decodeRGBA(val, ret);
            colors[IMGUI_COL_IDS[param]] = ImVec4((float)ret[0]/255.0f, (float)ret[1]/255.0f, (float)ret[2]/255.0f, (float)ret[3]/255.0f);
            continue;
        }
    }

    return true;
}

bool ThemeManager::decodeRGBA(std::string str, uint8_t out[4]) {
    if (str[0] != '#' || !std::all_of(str.begin() + 1, str.end(), ::isxdigit) || str.length() != 9) {
        return false;
    }
    out[0] = std::stoi(str.substr(1, 2), NULL, 16);
    out[1] = std::stoi(str.substr(3, 2), NULL, 16);
    out[2] = std::stoi(str.substr(5, 2), NULL, 16);
    out[3] = std::stoi(str.substr(7, 2), NULL, 16);
    return true;
}

std::vector<std::string> ThemeManager::getThemeNames() {
    std::vector<std::string> names;
    for (auto [name, theme] : themes) { names.push_back(name); }
    return names;
}

std::map<std::string, int> ThemeManager::IMGUI_COL_IDS = {
    {"Text",                    ImGuiCol_Text},
    {"TextDisabled",            ImGuiCol_TextDisabled},
    {"WindowBg",                ImGuiCol_WindowBg}, 
    {"ChildBg",                 ImGuiCol_ChildBg},
    {"PopupBg",                 ImGuiCol_PopupBg},
    {"Border",                  ImGuiCol_Border},
    {"BorderShadow",            ImGuiCol_BorderShadow},
    {"FrameBg",                 ImGuiCol_FrameBg},
    {"FrameBgHovered",          ImGuiCol_FrameBgHovered},
    {"FrameBgActive",           ImGuiCol_FrameBgActive},
    {"TitleBg",                 ImGuiCol_TitleBg},
    {"TitleBgActive",           ImGuiCol_TitleBgActive},
    {"TitleBgCollapsed",        ImGuiCol_TitleBgCollapsed},
    {"MenuBarBg",               ImGuiCol_MenuBarBg},
    {"ScrollbarBg",             ImGuiCol_ScrollbarBg},
    {"ScrollbarGrab",           ImGuiCol_ScrollbarGrab},
    {"ScrollbarGrabHovered",    ImGuiCol_ScrollbarGrabHovered},
    {"ScrollbarGrabActive",     ImGuiCol_ScrollbarGrabActive},
    {"CheckMark",               ImGuiCol_CheckMark},
    {"SliderGrab",              ImGuiCol_SliderGrab},
    {"SliderGrabActive",        ImGuiCol_SliderGrabActive},
    {"Button",                  ImGuiCol_Button},
    {"ButtonHovered",           ImGuiCol_ButtonHovered},
    {"ButtonActive",            ImGuiCol_ButtonActive},
    {"Header",                  ImGuiCol_Header},
    {"HeaderHovered",           ImGuiCol_HeaderHovered},
    {"HeaderActive",            ImGuiCol_HeaderActive},
    {"Separator",               ImGuiCol_Separator},
    {"SeparatorHovered",        ImGuiCol_SeparatorHovered},
    {"SeparatorActive",         ImGuiCol_SeparatorActive},
    {"ResizeGrip",              ImGuiCol_ResizeGrip},
    {"ResizeGripHovered",       ImGuiCol_ResizeGripHovered},
    {"ResizeGripActive",        ImGuiCol_ResizeGripActive},
    {"Tab",                     ImGuiCol_Tab},
    {"TabHovered",              ImGuiCol_TabHovered},
    {"TabActive",               ImGuiCol_TabActive},
    {"TabUnfocused",            ImGuiCol_TabUnfocused},
    {"TabUnfocusedActive",      ImGuiCol_TabUnfocusedActive},
    {"PlotLines",               ImGuiCol_PlotLines},
    {"PlotLinesHovered",        ImGuiCol_PlotLinesHovered},
    {"PlotHistogram",           ImGuiCol_PlotHistogram},
    {"PlotHistogramHovered",    ImGuiCol_PlotHistogramHovered},
    {"TableHeaderBg",           ImGuiCol_TableHeaderBg},
    {"TableBorderStrong",       ImGuiCol_TableBorderStrong},
    {"TableBorderLight",        ImGuiCol_TableBorderLight},
    {"TableRowBg",              ImGuiCol_TableRowBg},
    {"TableRowBgAlt",           ImGuiCol_TableRowBgAlt},
    {"TextSelectedBg",          ImGuiCol_TextSelectedBg},
    {"DragDropTarget",          ImGuiCol_DragDropTarget},
    {"NavHighlight",            ImGuiCol_NavHighlight},
    {"NavWindowingHighlight",   ImGuiCol_NavWindowingHighlight},
    {"NavWindowingDimBg",       ImGuiCol_NavWindowingDimBg},
    {"ModalWindowDimBg",        ImGuiCol_ModalWindowDimBg}
};