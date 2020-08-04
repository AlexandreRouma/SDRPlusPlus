#pragma once
#include <json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <filesystem>

using nlohmann::json;

namespace bandplan {
    struct Band_t {
        std::string name;
        std::string type;
        float start;
        float end;
    };

    void to_json(json& j, const Band_t& b);
    void from_json(const json& j, Band_t& b);

    struct BandPlan_t {
        std::string name;
        std::string countryName;
        std::string countryCode;
        std::string authorName;
        std::string authorURL;
        std::vector<Band_t> bands;
    };

    void to_json(json& j, const BandPlan_t& b);
    void from_json(const json& j, BandPlan_t& b);
    
    void loadBandPlan(std::string path);
    void loadFromDir(std::string path);

    extern std::map<std::string, BandPlan_t> bandplans;
    extern std::vector<std::string> bandplanNames;
    extern std::string bandplanNameTxt;
};