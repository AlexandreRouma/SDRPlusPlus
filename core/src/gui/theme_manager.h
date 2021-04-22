#pragma once
#include <map>
#include <mutex>
#include <imgui.h>
#include <vector>

struct Theme {
    std::string author;
    ImVec4 Text;
    ImVec4 TextDisabled;
    ImVec4 WindowBg;
    ImVec4 ChildBg;
    ImVec4 PopupBg;
    ImVec4 Border;
    ImVec4 BorderShadow;
    ImVec4 FrameBg;
    ImVec4 FrameBgHovered;
    ImVec4 FrameBgActive;
    ImVec4 TitleBg;
    ImVec4 TitleBgActive;
    ImVec4 TitleBgCollapsed;
    ImVec4 MenuBarBg;
    ImVec4 ScrollbarBg;
    ImVec4 ScrollbarGrab;
    ImVec4 ScrollbarGrabHovered;
    ImVec4 ScrollbarGrabActive;
    ImVec4 CheckMark;
    ImVec4 SliderGrab;
    ImVec4 SliderGrabActive;
    ImVec4 Button;
    ImVec4 ButtonHovered;
    ImVec4 ButtonActive;
    ImVec4 Header;
    ImVec4 HeaderHovered;
    ImVec4 HeaderActive;
    ImVec4 Separator;
    ImVec4 SeparatorHovered;
    ImVec4 SeparatorActive;
    ImVec4 ResizeGrip;
    ImVec4 ResizeGripHovered;
    ImVec4 ResizeGripActive;
    ImVec4 Tab;
    ImVec4 TabHovered;
    ImVec4 TabActive;
    ImVec4 TabUnfocused;
    ImVec4 TabUnfocusedActive;
    ImVec4 PlotLines;
    ImVec4 PlotLinesHovered;
    ImVec4 PlotHistogram;
    ImVec4 PlotHistogramHovered;
    ImVec4 TableHeaderBg;
    ImVec4 TableBorderStrong;
    ImVec4 TableBorderLight;
    ImVec4 TableRowBg;
    ImVec4 TableRowBgAlt;
    ImVec4 TextSelectedBg;
    ImVec4 DragDropTarget;
    ImVec4 NavHighlight;
    ImVec4 NavWindowingHighlight;
    ImVec4 NavWindowingDimBg;
    ImVec4 ModalWindowDimBg;
};

class ThemeManager {
public:
    bool loadThemesFromDir(std::string path);
    bool loadTheme(std::string path);
    bool applyTheme(std::string name);

    std::vector<std::string> getThemeNames();

private:
    static bool decodeRGBA(std::string str, uint8_t out[4]);

    std::map<std::string, Theme> themes;

};