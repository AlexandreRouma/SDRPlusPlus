#pragma once
#include <map>
#include <mutex>
#include <imgui.h>
#include <vector>
#include <json.hpp>

using nlohmann::json;

struct Theme {
    std::string author;
    json data;
};

class ThemeManager {
public:
    bool loadThemesFromDir(std::string path);
    bool loadTheme(std::string path);
    bool applyTheme(std::string name);

    std::vector<std::string> getThemeNames();

private:
    static bool decodeRGBA(std::string str, uint8_t out[4]);

    static std::map<std::string, int> IMGUI_COL_IDS;

    std::map<std::string, Theme> themes;

};