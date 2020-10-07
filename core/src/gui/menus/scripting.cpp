#include <gui/menus/scripting.h>
#include <core.h>
#include <gui/style.h>

namespace scriptingmenu {
    void init() {

    }

    void draw(void* ctx) {
        float menuWidth = ImGui::GetContentRegionAvailWidth();
        for (const auto& [name, script] : core::scriptManager.scripts) {
            bool running = script->running;
            if (running) { style::beginDisabled(); }
            if (ImGui::Button(name.c_str(), ImVec2(menuWidth, 0))) {
                script->run();
            }
            if (running) { style::endDisabled(); }
        }
    }
}