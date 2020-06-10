#pragma once
#include <cstdint>
#include <imgui.h>

namespace ImGui {
// Use this structure to pass the plot data and settings into the Plot function
struct PlotConfig {
    struct Values {
        // if necessary, you can provide x-axis values
        const float *xs = nullptr;
        // array of y values. If null, use ys_list (below)
        const float *ys = nullptr;
        // the number of values in each array
        int count;
        // at which offset to start plotting.
        // Warning: count+offset must be <= length of array!
        int offset = 0;
        // Plot color. If 0, use ImGuiCol_PlotLines.
        ImU32 color = 0;

        // in case you need to draw multiple plots at once, use this instead of ys
        const float **ys_list = nullptr;
        // the number of plots to draw
        int ys_count = 0;
        // colors for each plot
        const ImU32* colors = nullptr;
    } values;
    struct Scale {
        // Minimum plot value
        float min;
        // Maximum plot value
        float max;
        enum Type {
            Linear,
            Log10,
        };
        // How to scale the x-axis
        Type type = Linear;
    } scale;
    struct Tooltip {
        bool show = false;
        const char* format = "%g: %8.4g";
    } tooltip;
    struct Grid {
        bool show = false;
        float size = 100; // at which intervals to draw the grid
        int subticks = 10; // how many subticks in each tick
    } grid_x, grid_y;
    struct Selection {
        bool show = false;
        uint32_t* start = nullptr;
        uint32_t* length = nullptr;
        // "Sanitize" function. Give it selection length, and it will return
        // the "allowed" length. Useful for FFT, where selection must be
        // of power of two
        uint32_t(*sanitize_fn)(uint32_t) = nullptr;
    } selection;
    struct VerticalLines {
        bool show = false;
        const size_t* indices = nullptr; // at which indices to draw the lines
        size_t count = 0;
    } v_lines;
    ImVec2 frame_size = ImVec2(0.f, 0.f);
    float line_thickness = 1.f;
    bool skip_small_lines = true;
    const char* overlay_text = nullptr;
};

enum class PlotStatus {
    nothing,
    selection_updated,
};

IMGUI_API PlotStatus Plot(const char* label, const PlotConfig& conf);
}