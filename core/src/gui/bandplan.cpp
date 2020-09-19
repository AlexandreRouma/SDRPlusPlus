#include <gui/bandplan.h>

namespace bandplan {
    std::map<std::string, BandPlan_t> bandplans;
    std::vector<std::string> bandplanNames;
    std::string bandplanNameTxt;
    std::map<std::string, BandPlanColor_t> colorTable;

    void generateTxt() {
        bandplanNameTxt = "";
        for (int i = 0; i < bandplanNames.size(); i++) {
            bandplanNameTxt += bandplanNames[i];
            bandplanNameTxt += '\0';
        }
    }

    void to_json(json& j, const Band_t& b) {
        j = json{
            {"name", b.name},
            {"type", b.type},
            {"start", b.start},
            {"end", b.end},
        };
    }

    void from_json(const json& j, Band_t& b) {
        j.at("name").get_to(b.name);
        j.at("type").get_to(b.type);
        j.at("start").get_to(b.start);
        j.at("end").get_to(b.end);
    }

    void to_json(json& j, const BandPlan_t& b) {
        j = json{
            {"name", b.name},
            {"country_name", b.countryName},
            {"country_code", b.countryCode},
            {"author_name", b.authorName},
            {"author_url", b.authorURL},
            {"bands", b.bands}
        };
    }

    void from_json(const json& j, BandPlan_t& b) {
        j.at("name").get_to(b.name);
        j.at("country_name").get_to(b.countryName);
        j.at("country_code").get_to(b.countryCode);
        j.at("author_name").get_to(b.authorName);
        j.at("author_url").get_to(b.authorURL);
        j.at("bands").get_to(b.bands);
    }

    void to_json(json& j, const BandPlanColor_t& ct) {
        spdlog::error("ImGui color to JSON not implemented!!!");
    }
    
    void from_json(const json& j, BandPlanColor_t& ct) {
        std::string col = j.get<std::string>();
        if (col[0] != '#' || !std::all_of(col.begin() + 1, col.end(), ::isxdigit)) {
            return;
        }
        uint8_t r, g, b, a;
        r = std::stoi(col.substr(1, 2), NULL, 16);
        g = std::stoi(col.substr(3, 2), NULL, 16);
        b = std::stoi(col.substr(5, 2), NULL, 16);
        a = std::stoi(col.substr(7, 2), NULL, 16);
        ct.colorValue = IM_COL32(r, g, b, a);
        ct.transColorValue = IM_COL32(r, g, b, 100);
    }

    void loadBandPlan(std::string path) {
        std::ifstream file(path.c_str());
        json data;
        data << file;
        file.close();

        BandPlan_t plan = data.get<BandPlan_t>();
        if (bandplans.find(plan.name) != bandplans.end()) {
            spdlog::error("Duplicate band plan name ({0}), not loading.", plan.name);
            return;
        }
        bandplans[plan.name] = plan;
        bandplanNames.push_back(plan.name);
        generateTxt();
    }

    void loadFromDir(std::string path) {
        if (!std::filesystem::exists(path)) {
            spdlog::error("Band Plan directory does not exist");
            return;
        }
        if (!std::filesystem::is_directory(path)) {
            spdlog::error("Band Plan directory isn't a directory...");
            return;
        }
        bandplans.clear();
        for (const auto & file : std::filesystem::directory_iterator(path)) {
            std::string path = file.path().generic_string();
            if (file.path().extension().generic_string() != ".json") {
                continue;
            }
            loadBandPlan(path);
        }
    }

    void loadColorTable(std::string path) {
        if (!std::filesystem::exists(path)) {
            spdlog::error("Band Plan Color Table file does not exist");
            return;
        }
        if (!std::filesystem::is_regular_file(path)) {
            spdlog::error("Band Plan Color Table file isn't a file...");
            return;
        }
        std::ifstream file(path.c_str());
        json data;
        data << file;
        file.close();

        colorTable = data.get<std::map<std::string, BandPlanColor_t>>();
    }
};