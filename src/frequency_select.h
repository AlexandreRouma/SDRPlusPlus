#pragma once
#include <imgui.h>
#include <imgui_internal.h>

class FrequencySelect {
public:
    FrequencySelect();
    void init();
    void draw();
    void setFrequency(long freq);

    long frequency;
    bool frequencyChanged = false;

private:
    void onPosChange();
    void onResize();
    void incrementDigit(int i);
    void decrementDigit(int i);

    ImVec2 widgetPos;
    ImVec2 widgetEndPos;
    ImVec2 widgetSize;

    ImVec2 lastWidgetPos;
    ImVec2 lastWidgetSize;

    ImGuiWindow* window;
    ImFont* font;

    int digits[12];
    ImVec2 digitBottomMins[12];
    ImVec2 digitTopMins[12];
    ImVec2 digitBottomMaxs[12];
    ImVec2 digitTopMaxs[12];

    char buf[100];
};