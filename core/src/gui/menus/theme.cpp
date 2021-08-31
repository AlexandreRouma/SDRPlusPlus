#include <gui/menus/theme.h>
#include <gui/gui.h>
#include <options.h>
#include <core.h>
#include <gui/style.h>

namespace thememenu {
    int themeId;
    std::vector<std::string> themeNames;
    std::string themeNamesTxt;

    void init(std::string resDir) {
        // TODO: Not hardcode theme directory
        gui::themeManager.loadThemesFromDir(resDir + "/themes/");
        core::configManager.acquire();
        std::string selectedThemeName = core::configManager.conf["theme"];
        core::configManager.release();

        // Select theme by name, if not available, apply Dark theme
        themeNames = gui::themeManager.getThemeNames();
        auto it = std::find(themeNames.begin(), themeNames.end(), selectedThemeName);
        if (it == themeNames.end()) {
            it = std::find(themeNames.begin(), themeNames.end(), "Dark");
            selectedThemeName = "Dark";
        }
        gui::themeManager.applyTheme(selectedThemeName);
        themeId = std::distance(themeNames.begin(), it);

        themeNamesTxt = "";
        for (auto name : themeNames) {
            themeNamesTxt += name;
            themeNamesTxt += '\0';
        }
    }

    void draw(void* ctx) {
        float menuWidth = ImGui::GetContentRegionAvailWidth();
        ImGui::LeftLabel("Theme");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##theme_select_combo", &themeId, themeNamesTxt.c_str())) {
            gui::themeManager.applyTheme(themeNames[themeId]);
            core::configManager.acquire();
            core::configManager.conf["theme"] = themeNames[themeId];
            core::configManager.release(true);
        }
    }
}