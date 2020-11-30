#pragma once
#include <imgui.h>

namespace style {
    extern ImFont* baseFont;
    extern ImFont* bigFont;
    extern ImFont* hugeFont;

    void setDefaultStyle();
    void setDarkStyle();
    void beginDisabled();
    void endDisabled();
    void testtt();
}