#include <gui/menus/display.h>
#include <imgui.h>
#include <gui/gui.h>
#include <core.h>

namespace displaymenu {
    bool showWaterfall;

    void init() {
        showWaterfall = core::configManager.conf["showWaterfall"];
        showWaterfall ? gui::waterfall.showWaterfall() : gui::waterfall.hideWaterfall();
    }

    void draw(void* ctx) {
        if (ImGui::Checkbox("Show Waterfall", &showWaterfall)) {
            showWaterfall ? gui::waterfall.showWaterfall() : gui::waterfall.hideWaterfall();
            core::configManager.aquire();
            core::configManager.conf["showWaterfall"] = showWaterfall;
            core::configManager.release(true);
        }
    }
}