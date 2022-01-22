#include <gui/widgets/frequency_select.h>
#include <config.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <glfw_window.h>
#include <GLFW/glfw3.h>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui/imgui_internal.h>

bool isInArea(ImVec2 val, ImVec2 min, ImVec2 max) {
    return val.x >= min.x && val.x < max.x && val.y >= min.y && val.y < max.y;
}

FrequencySelect::FrequencySelect() {
}

void FrequencySelect::init() {
    for (int i = 0; i < 12; i++) {
        digits[i] = 0;
    }
}

void FrequencySelect::onPosChange() {
    ImVec2 digitSz = ImGui::CalcTextSize("0");
    ImVec2 commaSz = ImGui::CalcTextSize(".");
    int digitHeight = digitSz.y;
    int digitWidth = digitSz.x;
    int commaOffset = 0;
    for (int i = 0; i < 12; i++) {
        digitTopMins[i] = ImVec2(widgetPos.x + (i * digitWidth) + commaOffset, widgetPos.y);
        digitBottomMins[i] = ImVec2(widgetPos.x + (i * digitWidth) + commaOffset, widgetPos.y + (digitHeight / 2));

        digitTopMaxs[i] = ImVec2(widgetPos.x + (i * digitWidth) + commaOffset + digitWidth, widgetPos.y + (digitHeight / 2));
        digitBottomMaxs[i] = ImVec2(widgetPos.x + (i * digitWidth) + commaOffset + digitWidth, widgetPos.y + digitHeight);

        if ((i + 1) % 3 == 0 && i < 11) {
            commaOffset += commaSz.x;
        }
    }
}

void FrequencySelect::onResize() {
}

void FrequencySelect::incrementDigit(int i) {
    if (i < 0) {
        return;
    }
    if (digits[i] < 9) {
        digits[i]++;
    }
    else {
        digits[i] = 0;
        incrementDigit(i - 1);
    }
    frequencyChanged = true;
}

void FrequencySelect::decrementDigit(int i) {
    if (i < 0) {
        return;
    }
    if (digits[i] > 0) {
        digits[i]--;
    }
    else {
        if (i == 0) { return; }

        // Check if there are non zero digits afterwards
        bool otherNoneZero = false;
        for (int j = i - 1; j >= 0; j--) {
            if (digits[j] > 0) {
                otherNoneZero = true;
                break;
            }
        }
        if (!otherNoneZero) { return; }

        digits[i] = 9;
        decrementDigit(i - 1);
    }
    frequencyChanged = true;
}

void FrequencySelect::moveCursorToDigit(int i) {
    double xpos, ypos;
    glfwGetCursorPos(core::window, &xpos, &ypos);
    float nxpos = (digitTopMaxs[i].x + digitTopMins[i].x) / 2.0f;
    glfwSetCursorPos(core::window, nxpos, ypos);
}

void FrequencySelect::draw() {
    window = ImGui::GetCurrentWindow();
    widgetPos = ImGui::GetWindowContentRegionMin();
    widgetEndPos = ImGui::GetWindowContentRegionMax();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    widgetPos.x += window->Pos.x + cursorPos.x;
    widgetPos.y += window->Pos.y - 3;
    widgetEndPos.x += window->Pos.x + cursorPos.x;
    widgetEndPos.y += window->Pos.y - 3;
    widgetSize = ImVec2(widgetEndPos.x - widgetPos.x, widgetEndPos.y - widgetPos.y);

    ImGui::PushFont(style::bigFont);

    if (widgetPos.x != lastWidgetPos.x || widgetPos.y != lastWidgetPos.y) {
        lastWidgetPos = widgetPos;
        onPosChange();
    }
    if (widgetSize.x != lastWidgetSize.x || widgetSize.y != lastWidgetSize.y) {
        lastWidgetSize = widgetSize;
        onResize();
    }

    ImU32 disabledColor = ImGui::GetColorU32(ImGuiCol_Text, 0.3f);
    ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

    ImVec2 digitSz = ImGui::CalcTextSize("0");
    ImVec2 commaSz = ImGui::CalcTextSize(".");
    int digitWidth = digitSz.x;
    int commaOffset = 0;
    bool zeros = true;

    ImGui::ItemSize(ImRect(digitTopMins[0], ImVec2(digitBottomMaxs[11].x + 15, digitBottomMaxs[11].y)));

    for (int i = 0; i < 12; i++) {
        if (digits[i] != 0) {
            zeros = false;
        }
        sprintf(buf, "%d", digits[i]);
        window->DrawList->AddText(ImVec2(widgetPos.x + (i * digitWidth) + commaOffset, widgetPos.y),
                                  zeros ? disabledColor : textColor, buf);
        if ((i + 1) % 3 == 0 && i < 11) {
            commaOffset += commaSz.x;
            window->DrawList->AddText(ImVec2(widgetPos.x + (i * digitWidth) + commaOffset + 11, widgetPos.y),
                                      zeros ? disabledColor : textColor, ".");
        }
    }

    if (!gui::mainWindow.lockWaterfallControls) {
        ImVec2 mousePos = ImGui::GetMousePos();
        bool leftClick = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool rightClick = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        int mw = ImGui::GetIO().MouseWheel;
        bool onDigit = false;
        bool hovered = false;

        for (int i = 0; i < 12; i++) {
            onDigit = false;
            if (isInArea(mousePos, digitTopMins[i], digitTopMaxs[i])) {
                window->DrawList->AddRectFilled(digitTopMins[i], digitTopMaxs[i], IM_COL32(255, 0, 0, 75));
                if (leftClick) {
                    incrementDigit(i);
                }
                onDigit = true;
            }
            if (isInArea(mousePos, digitBottomMins[i], digitBottomMaxs[i])) {
                window->DrawList->AddRectFilled(digitBottomMins[i], digitBottomMaxs[i], IM_COL32(0, 0, 255, 75));
                if (leftClick) {
                    decrementDigit(i);
                }
                onDigit = true;
            }
            if (onDigit) {
                hovered = true;
                if (rightClick || (ImGui::IsKeyPressed(GLFW_KEY_DELETE) || ImGui::IsKeyPressed(GLFW_KEY_ENTER) || ImGui::IsKeyPressed(GLFW_KEY_KP_ENTER))) {
                    for (int j = i; j < 12; j++) {
                        digits[j] = 0;
                    }

                    frequencyChanged = true;
                }
                if (ImGui::IsKeyPressed(GLFW_KEY_UP)) {
                    incrementDigit(i);
                }
                if (ImGui::IsKeyPressed(GLFW_KEY_DOWN)) {
                    decrementDigit(i);
                }
                if ((ImGui::IsKeyPressed(GLFW_KEY_LEFT) || ImGui::IsKeyPressed(GLFW_KEY_BACKSPACE)) && i > 0) {
                    moveCursorToDigit(i - 1);
                }
                if (ImGui::IsKeyPressed(GLFW_KEY_RIGHT) && i < 11) {
                    moveCursorToDigit(i + 1);
                }

                auto chars = ImGui::GetIO().InputQueueCharacters;

                // For each keyboard characters, type it
                for (int j = 0; j < chars.Size; j++) {
                    if (chars[j] >= '0' && chars[j] <= '9') {
                        digits[i + j] = chars[j] - '0';
                        if ((i + j) < 11) { moveCursorToDigit(i + j + 1); }
                        frequencyChanged = true;
                    }
                }

                if (mw != 0) {
                    int count = abs(mw);
                    for (int j = 0; j < count; j++) {
                        mw > 0 ? incrementDigit(i) : decrementDigit(i);
                    }
                }
            }
        }
        digitHovered = hovered;
    }

    uint64_t freq = 0;
    for (int i = 0; i < 12; i++) {
        freq += digits[i] * pow(10, 11 - i);
    }

    uint64_t orig = freq;
    freq = std::clamp<uint64_t>(freq, minFreq, maxFreq);
    if (freq != orig && limitFreq) {
        setFrequency(freq);
    }
    else {
        frequency = orig;
    }

    ImGui::PopFont();

    ImGui::SetCursorPosX(digitBottomMaxs[11].x + 17);

    //ImGui::NewLine();
}

void FrequencySelect::setFrequency(int64_t freq) {
    freq = std::max<int64_t>(0, freq);
    int i = 11;
    for (uint64_t f = freq; i >= 0; i--) {
        digits[i] = f % 10;
        f -= digits[i];
        f /= 10;
    }
    frequency = freq;
}