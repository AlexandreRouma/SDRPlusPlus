#pragma once
#include <string>
#include <vector>
#include <module.h>
#include <map>

namespace colormaps {
    struct Map {
        std::string name;
        std::string author;
        float* map;
        int entryCount;
    };

    void loadMap(std::string path);

    SDRPP_EXPORT std::map<std::string, Map> maps;
}