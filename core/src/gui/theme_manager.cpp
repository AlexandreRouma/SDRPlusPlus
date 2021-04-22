#include <json.hpp>
#include <gui/theme_manager.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

using nlohmann::json;

bool ThemeManager::loadThemesFromDir(std::string path) {
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

    // Load JSON
    std::ifstream file(path.c_str());
    json data;
    file >> data;
    file.close();

    // Load theme
    uint8_t val[4];
    if (data.contains("Text")) {                    if (decodeRGBA(data["Text"], val)) {                    thm.Text = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TextDisabled")) {            if (decodeRGBA(data["TextDisabled"], val)) {            thm.TextDisabled = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("WindowBg")) {                if (decodeRGBA(data["WindowBg"], val)) {                thm.WindowBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ChildBg")) {                 if (decodeRGBA(data["ChildBg"], val)) {                 thm.ChildBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("PopupBg")) {                 if (decodeRGBA(data["PopupBg"], val)) {                 thm.PopupBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("Border")) {                  if (decodeRGBA(data["Border"], val)) {                  thm.Border = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("BorderShadow")) {            if (decodeRGBA(data["BorderShadow"], val)) {            thm.BorderShadow = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("FrameBg")) {                 if (decodeRGBA(data["FrameBg"], val)) {                 thm.FrameBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("FrameBgHovered")) {          if (decodeRGBA(data["FrameBgHovered"], val)) {          thm.FrameBgHovered = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("FrameBgActive")) {           if (decodeRGBA(data["FrameBgActive"], val)) {           thm.FrameBgActive = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TitleBg")) {                 if (decodeRGBA(data["TitleBg"], val)) {                 thm.TitleBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TitleBgActive")) {           if (decodeRGBA(data["TitleBgActive"], val)) {           thm.TitleBgActive = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TitleBgCollapsed")) {        if (decodeRGBA(data["TitleBgCollapsed"], val)) {        thm.TitleBgCollapsed = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("MenuBarBg")) {               if (decodeRGBA(data["MenuBarBg"], val)) {               thm.MenuBarBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ScrollbarBg")) {             if (decodeRGBA(data["ScrollbarBg"], val)) {             thm.ScrollbarBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ScrollbarGrab")) {           if (decodeRGBA(data["ScrollbarGrab"], val)) {           thm.ScrollbarGrab = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ScrollbarGrabHovered")) {    if (decodeRGBA(data["ScrollbarGrabHovered"], val)) {    thm.ScrollbarGrabHovered = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ScrollbarGrabActive")) {     if (decodeRGBA(data["ScrollbarGrabActive"], val)) {     thm.ScrollbarGrabActive = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("CheckMark")) {               if (decodeRGBA(data["CheckMark"], val)) {               thm.CheckMark = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("SliderGrab")) {              if (decodeRGBA(data["SliderGrab"], val)) {              thm.SliderGrab = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TeSliderGrabActivext")) {    if (decodeRGBA(data["SliderGrabActive"], val)) {        thm.SliderGrabActive = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("Button")) {                  if (decodeRGBA(data["Button"], val)) {                  thm.Button = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ButtonHovered")) {           if (decodeRGBA(data["ButtonHovered"], val)) {           thm.ButtonHovered = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ButtonActive")) {            if (decodeRGBA(data["ButtonActive"], val)) {            thm.ButtonActive = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("Header")) {                  if (decodeRGBA(data["Header"], val)) {                  thm.Header = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("HeaderHovered")) {           if (decodeRGBA(data["HeaderHovered"], val)) {           thm.HeaderHovered = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("HeaderActive")) {            if (decodeRGBA(data["HeaderActive"], val)) {            thm.HeaderActive = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("Separator")) {               if (decodeRGBA(data["Separator"], val)) {               thm.Separator = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("SeparatorHovered")) {        if (decodeRGBA(data["SeparatorHovered"], val)) {        thm.SeparatorHovered = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("SeparatorActive")) {         if (decodeRGBA(data["SeparatorActive"], val)) {         thm.SeparatorActive = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ResizeGrip")) {              if (decodeRGBA(data["ResizeGrip"], val)) {              thm.ResizeGrip = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ResizeGripHovered")) {       if (decodeRGBA(data["ResizeGripHovered"], val)) {       thm.ResizeGripHovered = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ResizeGripActive")) {        if (decodeRGBA(data["ResizeGripActive"], val)) {        thm.ResizeGripActive = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("Tab")) {                     if (decodeRGBA(data["Tab"], val)) {                     thm.Tab = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TabHovered")) {              if (decodeRGBA(data["TabHovered"], val)) {              thm.TabHovered = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TabActive")) {               if (decodeRGBA(data["TabActive"], val)) {               thm.TabActive = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TabUnfocused")) {            if (decodeRGBA(data["TabUnfocused"], val)) {            thm.TabUnfocused = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TabUnfocusedActive")) {      if (decodeRGBA(data["TabUnfocusedActive"], val)) {      thm.TabUnfocusedActive = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("PlotLines")) {               if (decodeRGBA(data["PlotLines"], val)) {               thm.PlotLines = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("PlotLinesHovered")) {        if (decodeRGBA(data["PlotLinesHovered"], val)) {        thm.PlotLinesHovered = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("PlotHistogram")) {           if (decodeRGBA(data["PlotHistogram"], val)) {           thm.PlotHistogram = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("PlotHistogramHovered")) {    if (decodeRGBA(data["PlotHistogramHovered"], val)) {    thm.PlotHistogramHovered = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TableHeaderBg")) {           if (decodeRGBA(data["TableHeaderBg"], val)) {           thm.TableHeaderBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TableBorderStrong")) {       if (decodeRGBA(data["TableBorderStrong"], val)) {       thm.TableBorderStrong = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TableBorderLight")) {        if (decodeRGBA(data["TableBorderLight"], val)) {        thm.TableBorderLight = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TableRowBg")) {              if (decodeRGBA(data["TableRowBg"], val)) {              thm.TableRowBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TableRowBgAlt")) {           if (decodeRGBA(data["TableRowBgAlt"], val)) {           thm.TableRowBgAlt = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("TextSelectedBg")) {          if (decodeRGBA(data["TextSelectedBg"], val)) {          thm.TextSelectedBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("DragDropTarget")) {          if (decodeRGBA(data["DragDropTarget"], val)) {          thm.DragDropTarget = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("NavHighlight")) {            if (decodeRGBA(data["NavHighlight"], val)) {            thm.NavHighlight = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("NavWindowingHighlight")) {   if (decodeRGBA(data["NavWindowingHighlight"], val)) {   thm.NavWindowingHighlight = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("NavWindowingDimBg")) {       if (decodeRGBA(data["NavWindowingDimBg"], val)) {       thm.NavWindowingDimBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }
    if (data.contains("ModalWindowDimBg")) {        if (decodeRGBA(data["ModalWindowDimBg"], val)) {        thm.ModalWindowDimBg = ImVec4((float)val[0]/255.0f, (float)val[1]/255.0f, (float)val[2]/255.0f, (float)val[3]/255.0f); } }

    return true;
}

bool ThemeManager::applyTheme(std::string name) {
    if (themes.find(name) == themes.end()) {
        spdlog::error("Unknown theme: {0}", name);
        return false;
    }

    auto& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    Theme thm = themes[name];
    colors[ImGuiCol_Text] = thm.Text;
    colors[ImGuiCol_TextDisabled] = thm.TextDisabled;
    colors[ImGuiCol_WindowBg] = thm.WindowBg;
    colors[ImGuiCol_ChildBg] = thm.ChildBg;
    colors[ImGuiCol_PopupBg] = thm.PopupBg;
    colors[ImGuiCol_Border] = thm.Border;
    colors[ImGuiCol_BorderShadow] = thm.BorderShadow;
    colors[ImGuiCol_FrameBg] = thm.FrameBg;
    colors[ImGuiCol_FrameBgHovered] = thm.FrameBgHovered;
    colors[ImGuiCol_FrameBgActive] = thm.FrameBgActive;
    colors[ImGuiCol_TitleBg] = thm.TitleBg;
    colors[ImGuiCol_TitleBgActive] = thm.TitleBgActive;
    colors[ImGuiCol_TitleBgCollapsed] = thm.TitleBgCollapsed;
    colors[ImGuiCol_MenuBarBg] = thm.MenuBarBg;
    colors[ImGuiCol_ScrollbarBg] = thm.ScrollbarBg;
    colors[ImGuiCol_ScrollbarGrab] = thm.ScrollbarGrab;
    colors[ImGuiCol_ScrollbarGrabHovered] = thm.ScrollbarGrabHovered;
    colors[ImGuiCol_ScrollbarGrabActive] = thm.ScrollbarGrabActive;
    colors[ImGuiCol_CheckMark] = thm.CheckMark;
    colors[ImGuiCol_SliderGrab] = thm.SliderGrab;
    colors[ImGuiCol_SliderGrabActive] = thm.SliderGrabActive;
    colors[ImGuiCol_Button] = thm.Button;
    colors[ImGuiCol_ButtonHovered] = thm.ButtonHovered;
    colors[ImGuiCol_ButtonActive] = thm.ButtonActive;
    colors[ImGuiCol_Header] = thm.Header;
    colors[ImGuiCol_HeaderHovered] = thm.HeaderHovered;
    colors[ImGuiCol_HeaderActive] = thm.HeaderActive;
    colors[ImGuiCol_Separator] = thm.Separator;
    colors[ImGuiCol_SeparatorHovered] = thm.SeparatorHovered;
    colors[ImGuiCol_SeparatorActive] = thm.SeparatorActive;
    colors[ImGuiCol_ResizeGrip] = thm.ResizeGrip;
    colors[ImGuiCol_ResizeGripHovered] = thm.ResizeGripHovered;
    colors[ImGuiCol_ResizeGripActive] = thm.ResizeGripActive;
    colors[ImGuiCol_Tab] = thm.Tab;
    colors[ImGuiCol_TabHovered] = thm.TabHovered;
    colors[ImGuiCol_TabActive] = thm.TabActive;
    colors[ImGuiCol_TabUnfocused] = thm.TabUnfocused;
    colors[ImGuiCol_TabUnfocusedActive] = thm.TabUnfocusedActive;
    colors[ImGuiCol_PlotLines] = thm.PlotLines;
    colors[ImGuiCol_PlotLinesHovered] = thm.PlotLinesHovered;
    colors[ImGuiCol_PlotHistogram] = thm.PlotHistogram;
    colors[ImGuiCol_PlotHistogramHovered] = thm.PlotHistogramHovered;
    colors[ImGuiCol_TableHeaderBg] = thm.TableHeaderBg;
    colors[ImGuiCol_TableBorderStrong] = thm.TableBorderStrong;
    colors[ImGuiCol_TableBorderLight] = thm.TableBorderLight;
    colors[ImGuiCol_TableRowBg] = thm.TableRowBg;
    colors[ImGuiCol_TableRowBgAlt] = thm.TableRowBgAlt;
    colors[ImGuiCol_TextSelectedBg] = thm.TextSelectedBg;
    colors[ImGuiCol_DragDropTarget] = thm.DragDropTarget;
    colors[ImGuiCol_NavHighlight] = thm.NavHighlight;
    colors[ImGuiCol_NavWindowingHighlight] = thm.NavWindowingHighlight;
    colors[ImGuiCol_NavWindowingDimBg] = thm.NavWindowingDimBg;
    colors[ImGuiCol_ModalWindowDimBg] = thm.ModalWindowDimBg;

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