#include <imgui.h>
#include <watcher.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <options.h>

#include <dsp/pll.h>
#include <dsp/stream.h>
#include <dsp/demodulator.h>
#include <dsp/window.h>
#include <dsp/resampling.h>
#include <dsp/processing.h>
#include <dsp/routing.h>
#include <dsp/sink.h>

#include <gui/widgets/folder_select.h>
#include <gui/widgets/constellation_diagram.h>

#include <sat_decoder.h>
#include <noaa_hrpt_decoder.h>

#define CONCAT(a, b)    ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "weather_sat_decoder",
    /* Description:     */ "Weather Satellite Decoder for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

std::string genFileName(std::string prefix, std::string suffix) {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buf[1024];
    sprintf(buf, "%s_%02d-%02d-%02d_%02d-%02d-%02d%s", prefix.c_str(), ltm->tm_hour, ltm->tm_min, ltm->tm_sec, ltm->tm_mday, ltm->tm_mon + 1, ltm->tm_year + 1900, suffix.c_str());
    return buf;
}

class WeatherSatDecoderModule : public ModuleManager::Instance {
public:
    WeatherSatDecoderModule(std::string name) {
        this->name = name;

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 1000000, 1000000, 1000000, 1000000, true);

        decoders["NOAA HRPT"] = new NOAAHRPTDecoder(vfo, name);

        // Generate the list
        decoderNames.clear();
        decoderNamesStr = "";
        for (auto const& [name, dec] : decoders) {
            decoderNames.push_back(name);
            decoderNamesStr += name;
            decoderNamesStr += '\0';
        }

        selectDecoder(decoderNames[0], false);
        
        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~WeatherSatDecoderModule() {
        decoder->stop();
    }

    void enable() {
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 1000000, 1000000, 1000000, 1000000, true);
        for (auto const& [name, dec] : decoders) { dec->setVFO(vfo); }
        decoder->select();
        decoder->start();
        enabled = true;
    }

    void disable() {
        // Stop decoder
        decoder->stop();

        sigpath::vfoManager.deleteVFO(vfo);
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    void selectDecoder(std::string name, bool deselectLast = true) {
        if (deselectLast) {
            decoder->stop();
        }
        decoder = decoders[name];
        decoder->select();
        decoder->start();
    }

    static void menuHandler(void* ctx) {
        WeatherSatDecoderModule* _this = (WeatherSatDecoderModule*)ctx;

        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (!_this->enabled) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo("##todo", &_this->decoderId, _this->decoderNamesStr.c_str())) {
            _this->selectDecoder(_this->decoderNames[_this->decoderId]);
        }

        _this->decoder->drawMenu(menuWidth);

        ImGui::Button("Record##testdsdfsds", ImVec2(menuWidth, 0));

        if (!_this->enabled) { style::endDisabled(); }
    }

    std::string name;
    bool enabled = true;
    
    VFOManager::VFO* vfo;

    std::map<std::string, SatDecoder*> decoders;
    std::vector<std::string> decoderNames;
    std::string decoderNamesStr = "";
    int decoderId = 0;

    SatDecoder* decoder;

};

MOD_EXPORT void _INIT_() {
    // Nothing
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new WeatherSatDecoderModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (WeatherSatDecoderModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing either
}