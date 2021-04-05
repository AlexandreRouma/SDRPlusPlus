#include <gui/widgets/combo_as_slider.h>
#include <imgui.h>
#include <imgui_internal.h>


namespace ImGui {
    bool ComboAsSlider(const char* label, int* currentItem, const char* itemsSeparatedByZeros){
        int itemsCount = 0;
        const char* p = itemsSeparatedByZeros;
        const char* currentLabel = nullptr;
        while (*p)
        {
            if(itemsCount == *currentItem) {
                currentLabel = p;
            }
            p += strlen(p) + 1;
            itemsCount++;
        }

        bool ret = ImGui::SliderInt(label, currentItem, 0, itemsCount-1, currentLabel);
        return ret;
    }
}
