#pragma once
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui_plot.h>
#include <dsp/resampling.h>
#include <dsp/demodulator.h>
#include <dsp/filter.h>
#include <thread>
#include <complex>
#include <dsp/source.h>
#include <dsp/math.h>
#include <waterfall.h>
#include <frequency_select.h>
#include <fftw3.h>
#include <signal_path.h>
#include <io/soapy.h>
#include <icons.h>
#include <bandplan.h>
#include <watcher.h>
#include <module.h>
#include <vfo_manager.h>

#define WINDOW_FLAGS    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground

void windowInit();
void drawWindow();
void bindVolumeVariable(float* vol);
void unbindVolumeVariable();