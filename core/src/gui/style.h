#pragma once
#include <imgui.h>
#include <string>

namespace style {
    extern ImFont* baseFont;
    extern ImFont* bigFont;
    extern ImFont* hugeFont;

    bool setDefaultStyle(std::string resDir);
    bool loadFonts(std::string resDir);
    void beginDisabled();
    void endDisabled();
    void testtt();
}