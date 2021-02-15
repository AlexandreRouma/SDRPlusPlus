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
        cbFuncs.EventCbFn = eventCB;
        cbFuncs.StreamACbFn = streamCB;
        cbFuncs.StreamBCbFn = streamCB;

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

        // if (sampleRateList.size() > 0) {
        //     sampleRate = sampleRateList[0];
        // }
        
        sampleRate = 8000000;

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
            switch (devArr[i].hwVer) {
                case SDRPLAY_RSP1_ID:
                    devListTxt += "RSP1 "; devListTxt += devArr[i].SerNo;
                    break;
                case SDRPLAY_RSP1A_ID:
                    devListTxt += "RSP1A "; devListTxt += devArr[i].SerNo;
                    break;
                case SDRPLAY_RSP2_ID:
                    devListTxt += "RSP2 "; devListTxt += devArr[i].SerNo;
                    break;
                case SDRPLAY_RSPduo_ID:
                    devListTxt += "RSPduo "; devListTxt += devArr[i].SerNo;
                    break;
                case SDRPLAY_RSPdx_ID:
                    devListTxt += "RSPdx "; devListTxt += devArr[i].SerNo;
                    break;
                default:
                    devListTxt += "Unknown "; devListTxt += devArr[i].SerNo;
                    break;
            }
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
            deviceOpen = false;
            return;
        }

        err = sdrplay_api_GetDeviceParams(openDev.dev, &openDevParams);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not get device params for RSP device: {0}", errStr);
            deviceOpen = false;
            return;
        }

        err = sdrplay_api_Init(openDev.dev, &cbFuncs, this);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not init RSP device: {0}", errStr);
            deviceOpen = false;
            return;
        }

        deviceOpen = true;
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
        sdrplay_api_ErrT err;

        _this->openDevParams->devParams->samplesPerPkt = 8000000 / 200;
        _this->openDevParams->devParams->fsFreq.fsHz = 8000000;
        _this->openDevParams->rxChannelA->tunerParams.bwType = sdrplay_api_BW_8_000;
        _this->openDevParams->rxChannelA->tunerParams.rfFreq.rfHz = _this->freq;
        _this->openDevParams->rxChannelA->tunerParams.gain.gRdB = 30;
        _this->openDevParams->rxChannelA->tunerParams.gain.LNAstate = 0;
        _this->openDevParams->rxChannelA->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
        //_this->openDevParams->devParams->

        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Dev_Fs, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_BwType, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Ctrl_Agc, sdrplay_api_Update_Ext1_None);

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
            _this->openDevParams->rxChannelA->tunerParams.rfFreq.rfHz = freq;
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None);
        }
        _this->freq = freq;
        spdlog::info("SDRPlaySourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        
       
        if (ImGui::Combo(CONCAT("##sdrplay_dev", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            
        }

        if (_this->running) { style::endDisabled(); } 

        if (_this->deviceOpen) {
            switch (_this->openDev.hwVer) {
                case SDRPLAY_RSP1_ID:
                    _this->RSP1Menu(menuWidth);
                    break;
                case SDRPLAY_RSP1A_ID:
                    _this->RSP1AMenu(menuWidth);
                    break;
                case SDRPLAY_RSP2_ID:
                    _this->RSP2Menu(menuWidth);
                    break;
                case SDRPLAY_RSPduo_ID:
                    _this->RSPduoMenu(menuWidth);
                    break;
                case SDRPLAY_RSPdx_ID:
                    _this->RSPdxMenu(menuWidth);
                    break;
                default:
                    _this->RSPUnsupportedMenu(menuWidth);
                    break;
            }
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No device available");
        }       
    }
        
    void RSP1Menu(float menuWidth) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Device currently unsupported");
    }

    void RSP1AMenu(float menuWidth) {
        
    }

    void RSP2Menu(float menuWidth) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Device currently unsupported");
    }

    void RSPduoMenu(float menuWidth) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Device currently unsupported");
    }

    void RSPdxMenu(float menuWidth) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Device currently unsupported");
    }

    void RSPUnsupportedMenu(float menuWidth) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Device currently unsupported");
    }

    static void streamCB(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                        unsigned int numSamples, unsigned int reset, void *cbContext) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)cbContext;
        if (!_this->running) { return; }
        for (int i = 0; i < numSamples; i++) {
            _this->stream.writeBuf[i].i = (float)xq[i] / 32768.0f;
            _this->stream.writeBuf[i].q = (float)xi[i] / 32768.0f;
        }
        _this->stream.swap(numSamples);
    }

    static void eventCB(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner,
                        sdrplay_api_EventParamsT *params, void *cbContext) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)cbContext;
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    bool deviceOpen = false;

    sdrplay_api_CallbackFnsT cbFuncs;

    sdrplay_api_DeviceT openDev;
    sdrplay_api_DeviceParamsT * openDevParams;

    int devId = 0;
    int srId = 0;

    int gainTest = 0;

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