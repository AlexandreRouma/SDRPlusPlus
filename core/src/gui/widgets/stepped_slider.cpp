#include <gui/widgets/stepped_slider.h>
#include <imgui.h>
#include <imgui_internal.h>


namespace ImGui {
    bool SliderFloatWithSteps(const char* label, float* v, float v_min, float v_max, float v_step, const char* display_format) {
        if (!display_format) {
            display_format = "%.3f";
        }

        char text_buf[64] = {};
        ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), display_format, *v);

        // Map from [v_min,v_max] to [0,N]
        const int countValues = int((v_max-v_min)/v_step);
        int v_i = int((*v - v_min)/v_step);
        bool value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf);

        // Remap from [0,N] to [v_min,v_max]
        *v = v_min + float(v_i) * v_step;
        return value_changed;
    }
}
