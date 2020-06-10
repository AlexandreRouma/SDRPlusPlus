#include <imgui_plot.h>
#include <imgui.h>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui_internal.h>

namespace ImGui {
// [0..1] -> [0..1]
static float rescale(float t, float min, float max, PlotConfig::Scale::Type type) {
    switch (type) {
    case PlotConfig::Scale::Linear:
        return t;
    case PlotConfig::Scale::Log10:
        return log10(ImLerp(min, max, t) / min) / log10(max / min);
    }
    return 0;
}

// [0..1] -> [0..1]
static float rescale_inv(float t, float min, float max, PlotConfig::Scale::Type type) {
    switch (type) {
    case PlotConfig::Scale::Linear:
        return t;
    case PlotConfig::Scale::Log10:
        return (pow(max/min, t) * min - min) / (max - min);
    }
    return 0;
}

static int cursor_to_idx(const ImVec2& pos, const ImRect& bb, const PlotConfig& conf, float x_min, float x_max) {
    const float t = ImClamp((pos.x - bb.Min.x) / (bb.Max.x - bb.Min.x), 0.0f, 0.9999f);
    const int v_idx = (int)(rescale_inv(t, x_min, x_max, conf.scale.type) * (conf.values.count - 1));
    IM_ASSERT(v_idx >= 0 && v_idx < conf.values.count);

    return v_idx;
}

PlotStatus Plot(const char* label, const PlotConfig& conf) {
    PlotStatus status = PlotStatus::nothing;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return status;

    const float* const* ys_list = conf.values.ys_list;
    int ys_count = conf.values.ys_count;
    const ImU32* colors = conf.values.colors;
    if (conf.values.ys != nullptr) { // draw only a single plot
        ys_list = &conf.values.ys;
        ys_count = 1;
        colors = &conf.values.color;
    }

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImRect frame_bb(
        window->DC.CursorPos,
        window->DC.CursorPos + conf.frame_size);
    const ImRect inner_bb(
        frame_bb.Min + style.FramePadding,
        frame_bb.Max - style.FramePadding);
    const ImRect total_bb = frame_bb;
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, 0, &frame_bb))
        return status;
    const bool hovered = ItemHoverable(frame_bb, id);

    RenderFrame(
        frame_bb.Min,
        frame_bb.Max,
        GetColorU32(ImGuiCol_WindowBg),
        true,
        style.FrameRounding);

    if (conf.values.count > 0) {
        int res_w;
        if (conf.skip_small_lines)
            res_w = ImMin((int)conf.frame_size.x, conf.values.count);
        else
            res_w = conf.values.count;
        res_w -= 1;
        int item_count = conf.values.count - 1;

        float x_min = conf.values.offset;
        float x_max = conf.values.offset + conf.values.count - 1;
        if (conf.values.xs) {
            x_min = conf.values.xs[size_t(x_min)];
            x_max = conf.values.xs[size_t(x_max)];
        }

        // Tooltip on hover
        int v_hovered = -1;
        if (conf.tooltip.show && hovered && inner_bb.Contains(g.IO.MousePos)) {
            const int v_idx = cursor_to_idx(g.IO.MousePos, inner_bb, conf, x_min, x_max);
            const size_t data_idx = conf.values.offset + (v_idx % conf.values.count);
            const float x0 = conf.values.xs ? conf.values.xs[data_idx] : v_idx;
            const float y0 = ys_list[0][data_idx]; // TODO: tooltip is only shown for the first y-value!
            SetTooltip(conf.tooltip.format, x0, y0);
            v_hovered = v_idx;
        }

        const float t_step = 1.0f / (float)res_w;
        const float inv_scale = (conf.scale.min == conf.scale.max) ?
                                    0.0f : (1.0f / (conf.scale.max - conf.scale.min));

        if (conf.grid_x.show) {
            int y0 = inner_bb.Min.y;
            int y1 = inner_bb.Max.y;
            switch (conf.scale.type) {
            case PlotConfig::Scale::Linear: {
                float cnt = conf.values.count / (conf.grid_x.size / conf.grid_x.subticks);
                float inc = 1.f / cnt;
                for (int i = 0; i <= cnt; ++i) {
                    int x0 = ImLerp(inner_bb.Min.x, inner_bb.Max.x, i * inc);
                    window->DrawList->AddLine(
                        ImVec2(x0, y0),
                        ImVec2(x0, y1),
                        IM_COL32(200, 200, 200, (i % conf.grid_x.subticks) ? 128 : 255));
                }
                break;
            }
            case PlotConfig::Scale::Log10: {
                float start = 1.f;
                while (start < x_max) {
                    for (int i = 1; i < 10; ++i) {
                        float x = start * i;
                        if (x < x_min) continue;
                        if (x > x_max) break;
                        float t = log10(x / x_min) / log10(x_max / x_min);
                        int x0 = ImLerp(inner_bb.Min.x, inner_bb.Max.x, t);
                        window->DrawList->AddLine(
                            ImVec2(x0, y0),
                            ImVec2(x0, y1),
                            IM_COL32(200, 200, 200, (i > 1) ? 128 : 255));
                    }
                    start *= 10.f;
                }
                break;
            }
            }
        }
        if (conf.grid_y.show) {
            int x0 = inner_bb.Min.x;
            int x1 = inner_bb.Max.x;
            float cnt = (conf.scale.max - conf.scale.min) / (conf.grid_y.size / conf.grid_y.subticks);
            float inc = 1.f / cnt;
            for (int i = 0; i <= cnt; ++i) {
                int y0 = ImLerp(inner_bb.Min.y, inner_bb.Max.y, i * inc);
                window->DrawList->AddLine(
                    ImVec2(x0, y0),
                    ImVec2(x1, y0),
                    IM_COL32(0, 0, 0, (i % conf.grid_y.subticks) ? 16 : 64));
            }
        }

        const ImU32 col_hovered = GetColorU32(ImGuiCol_PlotLinesHovered);
        ImU32 col_base = GetColorU32(ImGuiCol_PlotLines);

        for (int i = 0; i < ys_count; ++i) {
            if (colors) {
                if (colors[i]) col_base = colors[i];
                else col_base = GetColorU32(ImGuiCol_PlotLines);
            }
            float v0 = ys_list[i][conf.values.offset];
            float t0 = 0.0f;
            // Point in the normalized space of our target rectangle
            ImVec2 tp0 = ImVec2(t0, 1.0f - ImSaturate((v0 - conf.scale.min) * inv_scale));

            for (int n = 0; n < res_w; n++)
            {
                const float t1 = t0 + t_step;
                const int v1_idx = (int)(t0 * item_count + 0.5f);
                IM_ASSERT(v1_idx >= 0 && v1_idx < conf.values.count);
                const float v1 = ys_list[i][conf.values.offset + (v1_idx + 1) % conf.values.count];
                const ImVec2 tp1 = ImVec2(
                    rescale(t1, x_min, x_max, conf.scale.type),
                    1.0f - ImSaturate((v1 - conf.scale.min) * inv_scale));

            // NB: Draw calls are merged together by the DrawList system. Still, we should render our batch are lower level to save a bit of CPU.
                ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
                ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, tp1);

                if (v1_idx == v_hovered) {
                    window->DrawList->AddCircleFilled(pos0, 3, col_hovered);
                }

                window->DrawList->AddLine(
                    pos0,
                    pos1,
                    col_base,
                    conf.line_thickness);

                t0 = t1;
                tp0 = tp1;
            }
        }

        if (conf.v_lines.show) {
            for (size_t i = 0; i < conf.v_lines.count; ++i) {
                const size_t idx = conf.v_lines.indices[i];
                const float t1 = rescale(idx * t_step, x_min, x_max, conf.scale.type);
                ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, ImVec2(t1, 0.f));
                ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, ImVec2(t1, 1.f));
                window->DrawList->AddLine(pos0, pos1, IM_COL32(0xff, 0, 0, 0x88));
            }
        }

        if (conf.selection.show) {
            if (hovered) {
                if (g.IO.MouseClicked[0]) {
                    SetActiveID(id, window);
                    FocusWindow(window);

                    const int v_idx = cursor_to_idx(g.IO.MousePos, inner_bb, conf, x_min, x_max);
                    uint32_t start = conf.values.offset + (v_idx % conf.values.count);
                    uint32_t end = start;
                    if (conf.selection.sanitize_fn)
                        end = conf.selection.sanitize_fn(end - start) + start;
                    if (end < conf.values.offset + conf.values.count) {
                        *conf.selection.start = start;
                        *conf.selection.length = end - start;
                        status = PlotStatus::selection_updated;
                    }
                }
            }

            if (g.ActiveId == id) {
                if (g.IO.MouseDown[0]) {
                    const int v_idx = cursor_to_idx(g.IO.MousePos, inner_bb, conf, x_min, x_max);
                    const uint32_t start = *conf.selection.start;
                    uint32_t end = conf.values.offset + (v_idx % conf.values.count);
                    if (end > start) {
                        if (conf.selection.sanitize_fn)
                            end = conf.selection.sanitize_fn(end - start) + start;
                        if (end < conf.values.offset + conf.values.count) {
                            *conf.selection.length = end - start;
                            status = PlotStatus::selection_updated;
                        }
                    }
                } else {
                    ClearActiveID();
                }
            }
            float fSelectionStep = 1.0 / item_count;
            ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max,
                                 ImVec2(fSelectionStep * *conf.selection.start, 0.f));
            ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max,
                                ImVec2(fSelectionStep * (*conf.selection.start + *conf.selection.length), 1.f));
            window->DrawList->AddRectFilled(pos0, pos1, IM_COL32(128, 128, 128, 32));
            window->DrawList->AddRect(pos0, pos1, IM_COL32(128, 128, 128, 128));
        }
    }

    // Text overlay
    if (conf.overlay_text)
        RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, conf.overlay_text, NULL, NULL, ImVec2(0.5f,0.0f));

    return status;
}
}