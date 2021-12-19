#pragma once
#include <json.hpp>
#include <imgui/imgui.h>
#include <stdint.h>

using nlohmann::json;

namespace bandplan {
    struct Band_t {
        std::string name;
        std::string type;
        double start;
        double end;
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

    struct BandPlanColor_t {
        uint32_t colorValue;
        uint32_t transColorValue;
    };

    void to_json(json& j, const BandPlanColor_t& ct);
    void from_json(const json& j, BandPlanColor_t& ct);

    void loadBandPlan(std::string path);
    void loadFromDir(std::string path);
    void loadColorTable(json table);

    extern std::map<std::string, BandPlan_t> bandplans;
    extern std::vector<std::string> bandplanNames;
    extern std::string bandplanNameTxt;
    extern std::map<std::string, BandPlanColor_t> colorTable;
};