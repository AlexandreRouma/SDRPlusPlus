#pragma once
#include "imgui.h"

#define WINDOW_FLAGS    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground

void windowInit();
void drawWindow();
void setViewBandwidthSlider(float bandwidth);
bool sdrIsRunning();
void setFFTSize(int size);