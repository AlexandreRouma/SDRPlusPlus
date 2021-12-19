#include <gui/colormaps.h>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <fstream>
#include <json.hpp>

using nlohmann::json;

namespace colormaps {
    std::map<std::string, Map> maps;

    void loadMap(std::string path) {
        if (!std::filesystem::is_regular_file(path)) {
            spdlog::error("Could not load {0}, file doesn't exist", path);
            return;
        }

        std::ifstream file(path.c_str());
        json data;
        file >> data;
        file.close();

        Map map;
        std::vector<std::string> mapTxt;

        try {
            map.name = data["name"];
            map.author = data["author"];
            mapTxt = data["map"].get<std::vector<std::string>>();
        }
        catch (const std::exception&) {
            spdlog::error("Could not load {0}", path);
            return;
        }

        map.entryCount = mapTxt.size();
        map.map = new float[mapTxt.size() * 3];
        int i = 0;
        for (auto const& col : mapTxt) {
            uint8_t r, g, b, a;
            map.map[i * 3] = std::stoi(col.substr(1, 2), NULL, 16);
            map.map[(i * 3) + 1] = std::stoi(col.substr(3, 2), NULL, 16);
            map.map[(i * 3) + 2] = std::stoi(col.substr(5, 2), NULL, 16);
            i++;
        }

        maps[map.name] = map;
    }
}