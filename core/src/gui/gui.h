#pragma once
#include <gui/waterfall.h>
#include <gui/frequency_select.h>
#include <gui/menu.h>
#include <module.h>

namespace gui {
    SDRPP_EXPORT ImGui::WaterFall waterfall;
    SDRPP_EXPORT FrequencySelect freqSelect;
    SDRPP_EXPORT Menu menu;
};