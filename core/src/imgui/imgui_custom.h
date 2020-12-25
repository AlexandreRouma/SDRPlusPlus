#pragma once
#include "imgui.h"

namespace ImGui{
    IMGUI_API void VolumeBar(float volumeL, float volumeR, float *holdL, float *holdR, const ImVec2& size_arg);
};
