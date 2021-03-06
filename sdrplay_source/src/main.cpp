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

unsigned int sampleRates[] = {
    2000000,
    3000000,
    4000000,
    5000000,
    6000000,
    7000000,
    8000000,
    9000000,
    10000000
};

const char* sampleRatesTxt =
    "2MHz\0"
    "3MHz\0"
    "4MHz\0"
    "5MHz\0"
    "6MHz\0"
    "7MHz\0"
    "8MHz\0"
    "9MHz\0"
    "10MHz\0";

sdrplay_api_Bw_MHzT bandwidths[] = {
    sdrplay_api_BW_0_200,
    sdrplay_api_BW_0_300,
    sdrplay_api_BW_0_600,
    sdrplay_api_BW_1_536,
    sdrplay_api_BW_5_000,
    sdrplay_api_BW_6_000,
    sdrplay_api_BW_7_000,
    sdrplay_api_BW_8_000,
};

const char* bandwidthsTxt =
    "200KHz\0"
    "300KHz\0"
    "600KHz\0"
    "1.536MHz\0"
    "5MHz\0"
    "6MHz\0"
    "7MHz\0"
    "8MHz\0"
    "Auto\0";

sdrplay_api_Bw_MHzT preferedBandwidth[] = {
    sdrplay_api_BW_5_000,
    sdrplay_api_BW_5_000,
    sdrplay_api_BW_5_000,
    sdrplay_api_BW_5_000,
    sdrplay_api_BW_6_000,
    sdrplay_api_BW_7_000,
    sdrplay_api_BW_8_000,
    sdrplay_api_BW_8_000,
    sdrplay_api_BW_8_000
};

const sdrplay_api_Rsp2_AntennaSelectT rsp2_antennaPorts[] = {
    sdrplay_api_Rsp2_ANTENNA_A,
    sdrplay_api_Rsp2_ANTENNA_B,
};

const char* rsp2_antennaPortsTxt = "Port A\0Port B\0";

const sdrplay_api_RspDx_AntennaSelectT rspdx_antennaPorts[] = {
    sdrplay_api_RspDx_ANTENNA_A,
    sdrplay_api_RspDx_ANTENNA_B,
    sdrplay_api_RspDx_ANTENNA_C
};

const char* rspdx_antennaPortsTxt = "Port A\0Port B\0Port C\0";



class SDRPlaySourceModule : public ModuleManager::Instance {
public:
    SDRPlaySourceModule(std::string name) {
        this->name = name;

        // Init callbacks
        cbFuncs.EventCbFn = eventCB;
        cbFuncs.StreamACbFn = streamCB;
        cbFuncs.StreamBCbFn = streamCB;

        sdrplay_api_Open();

        sampleRate = 2000000.0;
        srId = 0;

        bandwidth = sdrplay_api_BW_5_000;
        bandwidthId = 8;

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

    void selectById(int id) {
        selectDev(devList[id]);
    }

    void selectDev(sdrplay_api_DeviceT dev) {
        openDev = dev;
        sdrplay_api_ErrT err;

        if (deviceOpen) {
            sdrplay_api_Uninit(openDev.dev);
            sdrplay_api_ReleaseDevice(&openDev);
        }

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

        _this->bufferIndex = 0;
        _this->bufferSize = 8000000 / 200;

        // General options
        _this->openDevParams->devParams->fsFreq.fsHz = _this->sampleRate;
        _this->openDevParams->rxChannelA->tunerParams.bwType = _this->bandwidth;
        _this->openDevParams->rxChannelA->tunerParams.rfFreq.rfHz = _this->freq;
        _this->openDevParams->rxChannelA->tunerParams.gain.gRdB = _this->gain;
        _this->openDevParams->rxChannelA->tunerParams.gain.LNAstate = _this->lnaGain;
        _this->openDevParams->rxChannelA->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Dev_Fs, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_BwType, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Ctrl_Agc, sdrplay_api_Update_Ext1_None);

        // RSP1A Options
        if (_this->openDev.hwVer == SDRPLAY_RSP1A_ID) {
            _this->openDevParams->devParams->rsp1aParams.rfNotchEnable = _this->rsp1a_fmNotch;
            _this->openDevParams->devParams->rsp1aParams.rfNotchEnable = _this->rsp1a_dabNotch;
            _this->openDevParams->rxChannelA->rsp1aTunerParams.biasTEnable = _this->rsp1a_biasT;
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp1a_RfNotchControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp1a_RfDabNotchControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp1a_BiasTControl, sdrplay_api_Update_Ext1_None);
        }
        else if (_this->openDev.hwVer == SDRPLAY_RSP2_ID) {
            _this->openDevParams->rxChannelA->rsp2TunerParams.rfNotchEnable = _this->rsp2_notch;
            _this->openDevParams->rxChannelA->rsp2TunerParams.biasTEnable = _this->rsp2_biasT;
            _this->openDevParams->rxChannelA->rsp2TunerParams.antennaSel = rsp2_antennaPorts[_this->rsp2_antennaPort];
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_RfNotchControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_BiasTControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_AntennaControl, sdrplay_api_Update_Ext1_None);
        }
        else if (_this->openDev.hwVer == SDRPLAY_RSPdx_ID) {
            _this->openDevParams->devParams->rspDxParams.rfNotchEnable = _this->rspdx_fmNotch;
            _this->openDevParams->devParams->rspDxParams.rfNotchEnable = _this->rspdx_dabNotch;
            _this->openDevParams->devParams->rspDxParams.biasTEnable = _this->rspdx_biasT;
            _this->openDevParams->devParams->rspDxParams.antennaSel = rspdx_antennaPorts[_this->rspdx_antennaPort];
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfNotchControl);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfDabNotchControl);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_BiasTControl);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_AntennaControl);
        }

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
            _this->selectById(_this->devId);
            // Save config
        }


        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##sdrplay_sr", _this->name), &_this->srId, sampleRatesTxt)) {
            _this->sampleRate = sampleRates[_this->srId];
            if (_this->bandwidthId == 8) {
                _this->bandwidth = preferedBandwidth[_this->srId];
            }
            core::setInputSampleRate(_this->sampleRate);
            // Save config
        }

        if (_this->running) { style::endDisabled(); } 

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##sdrplay_bw", _this->name), &_this->bandwidthId, bandwidthsTxt)) {
            _this->bandwidth = (_this->bandwidthId == 8) ? preferedBandwidth[_this->srId] : bandwidths[_this->bandwidthId];
            if (_this->running) {
                _this->openDevParams->rxChannelA->tunerParams.bwType = _this->bandwidth;
                sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_BwType, sdrplay_api_Update_Ext1_None);
            }
            // Save config
        }

        if (_this->deviceOpen) {
            ImGui::PushItemWidth(menuWidth - ImGui::CalcTextSize("LNA Gain").x - 10);
            ImGui::Text("LNA Gain");
            ImGui::SameLine();
            float pos = ImGui::GetCursorPosX();
            if (ImGui::SliderInt(CONCAT("##sdrplay_lna_gain", _this->name), &_this->lnaGain, 9, 0, "")) {
                _this->openDevParams->rxChannelA->tunerParams.gain.LNAstate = _this->lnaGain;
                sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
            }

            ImGui::Text("IF Gain");
            ImGui::SameLine();
            ImGui::SetCursorPosX(pos);
            if (ImGui::SliderInt(CONCAT("##sdrplay_gain", _this->name), &_this->gain, 59, 20, "")) {
                _this->openDevParams->rxChannelA->tunerParams.gain.gRdB = _this->gain;
                sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
            }
            ImGui::PopItemWidth();

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
        if (ImGui::Checkbox(CONCAT("FM Notch##sdrplay_rsp1a_fmnotch", name), &rsp1a_fmNotch)) {
            openDevParams->devParams->rsp1aParams.rfNotchEnable = rsp1a_fmNotch;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp1a_RfNotchControl, sdrplay_api_Update_Ext1_None);
        }
        if (ImGui::Checkbox(CONCAT("DAB Notch##sdrplay_rsp1a_dabnotch", name), &rsp1a_dabNotch)) {
            openDevParams->devParams->rsp1aParams.rfNotchEnable = rsp1a_dabNotch;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp1a_RfDabNotchControl, sdrplay_api_Update_Ext1_None);
        }
        if (ImGui::Checkbox(CONCAT("Bias-T##sdrplay_rsp1a_biast", name), &rsp1a_biasT)) {
            openDevParams->rxChannelA->rsp1aTunerParams.biasTEnable = rsp1a_biasT;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp1a_BiasTControl, sdrplay_api_Update_Ext1_None);
        }
    }

    void RSP2Menu(float menuWidth) {
        if (ImGui::Checkbox(CONCAT("MW/FM Notch##sdrplay_rsp2_notch", name), &rsp2_notch)) {
            openDevParams->rxChannelA->rsp2TunerParams.rfNotchEnable = rsp2_notch;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_RfNotchControl, sdrplay_api_Update_Ext1_None);
        }
        if (ImGui::Checkbox(CONCAT("Bias-T##sdrplay_rsp2_biast", name), &rsp2_biasT)) {
            openDevParams->rxChannelA->rsp2TunerParams.biasTEnable = rsp2_biasT;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_BiasTControl, sdrplay_api_Update_Ext1_None);
        }
        if (ImGui::Combo(CONCAT("Antenna##sdrplay_rsp2_ant", name), &rsp2_antennaPort, rsp2_antennaPortsTxt)) {
            openDevParams->rxChannelA->rsp2TunerParams.antennaSel = rsp2_antennaPorts[rsp2_antennaPort];
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_AntennaControl, sdrplay_api_Update_Ext1_None);
        }
    }

    void RSPduoMenu(float menuWidth) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Device currently unsupported");
    }

    void RSPdxMenu(float menuWidth) {
        if (ImGui::Checkbox(CONCAT("FM Notch##sdrplay_rspdx_fmnotch", name), &rspdx_fmNotch)) {
            openDevParams->devParams->rspDxParams.rfNotchEnable = rspdx_fmNotch;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfNotchControl);
        }
        if (ImGui::Checkbox(CONCAT("DAB Notch##sdrplay_rspdx_dabnotch", name), &rspdx_dabNotch)) {
            openDevParams->devParams->rspDxParams.rfNotchEnable = rspdx_dabNotch;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfDabNotchControl);
        }
        if (ImGui::Checkbox(CONCAT("Bias-T##sdrplay_rspdx_biast", name), &rspdx_biasT)) {
            openDevParams->devParams->rspDxParams.biasTEnable = rspdx_biasT;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_BiasTControl);
        }
        if (ImGui::Combo(CONCAT("Antenna##sdrplay_rspdx_ant", name), &rspdx_antennaPort, rspdx_antennaPortsTxt)) {
            openDevParams->devParams->rspDxParams.antennaSel = rspdx_antennaPorts[rspdx_antennaPort];
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_AntennaControl);
        }
    }

    void RSPUnsupportedMenu(float menuWidth) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Device currently unsupported");
    }

    static void streamCB(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                        unsigned int numSamples, unsigned int reset, void *cbContext) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)cbContext;
        if (!_this->running) { return; }
        for (int i = 0; i < numSamples; i++) {
            int id = _this->bufferIndex++;
            _this->stream.writeBuf[id].i = (float)xq[i] / 32768.0f;
            _this->stream.writeBuf[id].q = (float)xi[i] / 32768.0f;

            if (_this->bufferIndex >= _this->bufferSize) {
                _this->stream.swap(_this->bufferSize);
                _this->bufferIndex = 0;
            }
        }
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

    sdrplay_api_Bw_MHzT bandwidth;
    int bandwidthId = 0;

    int devId = 0;
    int srId = 0;

    int lnaGain = 9;
    int gain = 59;

    int bufferSize = 0;
    int bufferIndex = 0;

    // RSP1A Options
    bool rsp1a_fmNotch = false;
    bool rsp1a_dabNotch = false;
    bool rsp1a_biasT = false;

    // RSP2 Options
    bool rsp2_notch = false;
    bool rsp2_biasT = false;
    int rsp2_antennaPort = 0;

    // RSPdx Options
    bool rspdx_fmNotch = false;
    bool rspdx_dabNotch = false;
    bool rspdx_biasT = false;
    int rspdx_antennaPort = 0;

    std::vector<sdrplay_api_DeviceT> devList;
    std::string devListTxt;
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