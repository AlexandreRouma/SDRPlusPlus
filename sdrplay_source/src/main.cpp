#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <options.h>
#include <sdrplay_api.h>


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "sdrplay_source",
    /* Description:     */ "Airspy source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class SDRPlaySourceModule : public ModuleManager::Instance {
public:
    SDRPlaySourceModule(std::string name) {
        this->name = name;

        // Init callbacks

        sdrplay_api_Open();

        sampleRate = 10000000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected; 
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        refresh();
        selectFirst();

        if (sampleRateList.size() > 0) {
            sampleRate = sampleRateList[0];
        }

        // Select device from config
        // config.aquire();
        // std::string devSerial = config.conf["device"];
        // config.release();
        // selectByString(devSerial);
        // core::setInputSampleRate(sampleRate);

        sigpath::sourceManager.registerSource("SDRplay", &handler);
    }

    ~SDRPlaySourceModule() {
        sdrplay_api_Close();
    }

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

    void refresh() {
        devList.clear();
        devListTxt = "";

        sdrplay_api_DeviceT devArr[128];
        unsigned int numDev = 0;
        sdrplay_api_GetDevices(devArr, &numDev, 128);

        for (unsigned int i = 0; i < numDev; i++) {
            devList.push_back(devArr[i]);
            devListTxt += devArr[i].SerNo;
            devListTxt += '\0';
        }
    }

    void selectFirst() {
        if (devList.size() == 0) { return; }
        selectDev(devList[0]);
    }

    void selectDev(sdrplay_api_DeviceT dev) {
        openDev = dev;
        sdrplay_api_ErrT err;

        err = sdrplay_api_SelectDevice(&openDev);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not select RSP device: {0}", errStr);
            return;
        }

        err = sdrplay_api_GetDeviceParams(openDev.dev, &openDevParams);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not get device params for RSP device: {0}", errStr);
            return;
        }



        spdlog::info("Init OK");
    }

private:
    std::string getBandwdithScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            sprintf(buf, "%.1lfMHz", bw / 1000000.0);
        }
        else if (bw >= 1000.0) {
            sprintf(buf, "%.1lfKHz", bw / 1000.0);
        }
        else {
            sprintf(buf, "%.1lfHz", bw);
        }
        return std::string(buf);
    }

    static void menuSelected(void* ctx) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("SDRPlaySourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)ctx;
        spdlog::info("SDRPlaySourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)ctx;
        if (_this->running) {
            return;
        }
        
        // Do start procedure here

        _this->running = true;
        spdlog::info("SDRPlaySourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)ctx;
        if (!_this->running) {
            return;
        }
        _this->running = false;
        _this->stream.stopWriter();
        
        // Stop procedure here

        _this->stream.clearWriteStop();
        spdlog::info("SDRPlaySourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)ctx;
        if (_this->running) {
            // Tune here
        }
        _this->freq = freq;
        spdlog::info("SDRPlaySourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        
        if (_this->running) { style::beginDisabled(); }
        if (ImGui::Combo(CONCAT("##sdrplay_dev", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            
        }

        if (_this->running) { style::endDisabled(); }

        
    }

    static void eventCB(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                        unsigned int numSamples, unsigned int reset, void *cbContext) {
        // Code here
    }

    static void tunerCB(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner,
                        sdrplay_api_EventParamsT *params, void *cbContext) {
        // Code here
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;

    sdrplay_api_CallbackFnsT cnFuncs;

    sdrplay_api_DeviceT openDev;
    sdrplay_api_DeviceParamsT * openDevParams;

    int devId = 0;
    int srId = 0;

    std::vector<sdrplay_api_DeviceT> devList;
    std::string devListTxt;
    std::vector<uint32_t> sampleRateList;
    std::string sampleRateListTxt;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(options::opts.root + "/sdrplay_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SDRPlaySourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (SDRPlaySourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}