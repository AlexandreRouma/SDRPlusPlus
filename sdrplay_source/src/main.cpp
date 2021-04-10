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
    /* Description:     */ "SDRplay source module for SDR++",
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
    sdrplay_api_Rsp2_ANTENNA_B,
};

const char* rsp2_antennaPortsTxt = "Port A\0Port B\0Hi-Z\0";

const sdrplay_api_RspDx_AntennaSelectT rspdx_antennaPorts[] = {
    sdrplay_api_RspDx_ANTENNA_A,
    sdrplay_api_RspDx_ANTENNA_B,
    sdrplay_api_RspDx_ANTENNA_C
};

const char* rspdx_antennaPortsTxt = "Port A\0Port B\0Port C\0";

const sdrplay_api_AgcControlT agcModes[] = {
    sdrplay_api_AGC_DISABLE,
    sdrplay_api_AGC_5HZ,
    sdrplay_api_AGC_50HZ,
    sdrplay_api_AGC_100HZ
};

const char* agcModesTxt = "Off\0005Hz\00050Hz\000100Hz";

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

        config.aquire();
        std::string confSelectDev = config.conf["device"];
        config.release();
        selectByName(confSelectDev);

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
        devNameList.clear();
        devListTxt = "";

        sdrplay_api_DeviceT devArr[128];
        unsigned int numDev = 0;
        sdrplay_api_GetDevices(devArr, &numDev, 128);

        for (unsigned int i = 0; i < numDev; i++) {
            devList.push_back(devArr[i]);
            std::string name = "";
            switch (devArr[i].hwVer) {
                case SDRPLAY_RSP1_ID:
                    name = "RSP1 ("; name += devArr[i].SerNo; name += ')';
                    break;
                case SDRPLAY_RSP1A_ID:
                    name = "RSP1A ("; name += devArr[i].SerNo; name += ')';
                    break;
                case SDRPLAY_RSP2_ID:
                    name = "RSP2 ("; name += devArr[i].SerNo; name += ')';
                    break;
                case SDRPLAY_RSPduo_ID:
                    name = "RSPduo ("; name += devArr[i].SerNo; name += ')';
                    break;
                case SDRPLAY_RSPdx_ID:
                    name = "RSPdx ("; name += devArr[i].SerNo; name += ')';
                    break;
                default:
                    name = "Unknown ("; name += devArr[i].SerNo; name += ')';
                    break;
            }
            devNameList.push_back(name);
            devListTxt += name;
            devListTxt += '\0';
        }
    }

    void selectFirst() {
        if (devList.size() == 0) { return; }
        selectDev(devList[0], 0);
    }

    void selectByName(std::string name) {
        for (int i = 0; i < devNameList.size(); i++) {
            if (devNameList[i] == name) {
                selectDev(devList[i], i);
                return;
            }
        }
        selectFirst();
    }

    void selectById(int id) {
        selectDev(devList[id], id);
    }

    void selectDev(sdrplay_api_DeviceT dev, int id) {
        openDev = dev;
        sdrplay_api_ErrT err;

        if (deviceOpen) {
            // TODO: Fix crash here
            sdrplay_api_Uninit(openDev.dev);
            sdrplay_api_ReleaseDevice(&openDev);
        }

        err = sdrplay_api_SelectDevice(&openDev);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not select RSP device: {0}", errStr);
            deviceOpen = false;
            selectedName = "";
            return;
        }

        err = sdrplay_api_GetDeviceParams(openDev.dev, &openDevParams);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not get device params for RSP device: {0}", errStr);
            deviceOpen = false;
            selectedName = "";
            return;
        }

        err = sdrplay_api_Init(openDev.dev, &cbFuncs, this);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not init RSP device: {0}", errStr);
            deviceOpen = false;
            selectedName = "";
            return;
        }

        selectedName = devNameList[id];

        if (openDev.hwVer == SDRPLAY_RSP1_ID) {
            lnaSteps = 4;
        }
        else if (openDev.hwVer == SDRPLAY_RSP1A_ID) {
            lnaSteps = 10;
        }
        else if (openDev.hwVer == SDRPLAY_RSP2_ID) {
            lnaSteps = 9;
        }
        else if (openDev.hwVer == SDRPLAY_RSPduo_ID) {
            lnaSteps = 10;
        }
        else if (openDev.hwVer == SDRPLAY_RSPdx_ID) {
            lnaSteps = 28;
        }

        bool created = false;
        config.aquire();
        if (!config.conf["devices"].contains(selectedName)) {
            created = true;
            config.conf["devices"][selectedName]["sampleRate"] = sampleRates[0];
            config.conf["devices"][selectedName]["bwMode"] = 8; // Auto
            config.conf["devices"][selectedName]["lnaGain"] = lnaSteps - 1;
            config.conf["devices"][selectedName]["ifGain"] = 59;
            config.conf["devices"][selectedName]["agc"] = 0; // Disabled

            if (openDev.hwVer == SDRPLAY_RSP1_ID) {
                // No config to load
            }
            else if (openDev.hwVer == SDRPLAY_RSP1A_ID) {
                config.conf["devices"][selectedName]["fmNotch"] = false;
                config.conf["devices"][selectedName]["dabNotch"] = false;
                config.conf["devices"][selectedName]["biast"] = false;
            }
            else if (openDev.hwVer == SDRPLAY_RSP2_ID) {
                config.conf["devices"][selectedName]["antenna"] = 0;
                config.conf["devices"][selectedName]["notch"] = false;
                config.conf["devices"][selectedName]["biast"] = false;
            }
            else if (openDev.hwVer == SDRPLAY_RSPduo_ID) {
                config.conf["devices"][selectedName]["fmNotch"] = false;
                config.conf["devices"][selectedName]["dabNotch"] = false;
                config.conf["devices"][selectedName]["biast"] = false;
            }
            else if (openDev.hwVer == SDRPLAY_RSPdx_ID) {
                config.conf["devices"][selectedName]["antenna"] = 0;
                config.conf["devices"][selectedName]["fmNotch"] = false;
                config.conf["devices"][selectedName]["dabNotch"] = false;
                config.conf["devices"][selectedName]["biast"] = false;
            }
        }

        // General options
        if (config.conf["devices"][selectedName].contains("sampleRate")) {
            sampleRate = config.conf["devices"][selectedName]["sampleRate"];
            bool found = false;
            for (int i = 0; i < 9; i++) {
                if (sampleRates[i] == sampleRate) {
                    srId = i;
                    found = true;
                }
            }
            if (!found) {
                sampleRate = sampleRates[0];
                srId = 0;
            }
        }
        if (config.conf["devices"][selectedName].contains("bwMode")) {
            bandwidthId = config.conf["devices"][selectedName]["bwMode"];
        }
        if (config.conf["devices"][selectedName].contains("lnaGain")) {
            lnaGain = config.conf["devices"][selectedName]["lnaGain"];
        }
        if (config.conf["devices"][selectedName].contains("ifGain")) {
            gain = config.conf["devices"][selectedName]["ifGain"];
        }
        if (config.conf["devices"][selectedName].contains("agc")) {
            agc = config.conf["devices"][selectedName]["agc"];
        }

        // Per device options
        if (openDev.hwVer == SDRPLAY_RSP1_ID) {
            // No config to load
        }
        else if (openDev.hwVer == SDRPLAY_RSP1A_ID) {
            if (config.conf["devices"][selectedName].contains("fmNotch")) {
                rsp1a_fmNotch = config.conf["devices"][selectedName]["fmNotch"];
            }
            if (config.conf["devices"][selectedName].contains("dabNotch")) {
                rsp1a_dabNotch = config.conf["devices"][selectedName]["dabNotch"];
            }
            if (config.conf["devices"][selectedName].contains("biast")) {
                rsp1a_biasT = config.conf["devices"][selectedName]["biast"];
            }
        }
        else if (openDev.hwVer == SDRPLAY_RSP2_ID) {
            if (config.conf["devices"][selectedName].contains("antenna")) {
                rsp2_antennaPort = config.conf["devices"][selectedName]["antenna"];
            }
            if (config.conf["devices"][selectedName].contains("notch")) {
                rsp2_notch = config.conf["devices"][selectedName]["notch"];
            }
            if (config.conf["devices"][selectedName].contains("biast")) {
                rsp2_biasT = config.conf["devices"][selectedName]["biast"];
            }
        }
        else if (openDev.hwVer == SDRPLAY_RSPduo_ID) {
            if (config.conf["devices"][selectedName].contains("fmNotch")) {
                rspduo_fmNotch = config.conf["devices"][selectedName]["fmNotch"];
            }
            if (config.conf["devices"][selectedName].contains("dabNotch")) {
                rspduo_dabNotch = config.conf["devices"][selectedName]["dabNotch"];
            }
            if (config.conf["devices"][selectedName].contains("biast")) {
                rspduo_biasT = config.conf["devices"][selectedName]["biast"];
            }
        }
        else if (openDev.hwVer == SDRPLAY_RSPdx_ID) {
            if (config.conf["devices"][selectedName].contains("antenna")) {
                rspdx_antennaPort = config.conf["devices"][selectedName]["antenna"];
            }
            if (config.conf["devices"][selectedName].contains("fmNotch")) {
                rspdx_fmNotch = config.conf["devices"][selectedName]["fmNotch"];
            }
            if (config.conf["devices"][selectedName].contains("dabNotch")) {
                rspdx_dabNotch = config.conf["devices"][selectedName]["dabNotch"];
            }
            if (config.conf["devices"][selectedName].contains("biast")) {
                rspdx_biasT = config.conf["devices"][selectedName]["biast"];
            }
        }

        config.release(created);
        
        if (lnaGain >= lnaSteps) { lnaGain = lnaSteps - 1; }

        deviceOpen = true;
    }
    
    void selectShittyTuner(sdrplay_api_TunerSelectT tuner, sdrplay_api_RspDuo_AmPortSelectT amPort) {
        // What the fuck?
        if (openDev.tuner != tuner) { sdrplay_api_SwapRspDuoActiveTuner(openDev.dev, &openDev.tuner, amPort); }
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
        _this->openDevParams->rxChannelA->ctrlParams.agc.enable = agcModes[_this->agc];
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
            _this->openDevParams->rxChannelA->rsp2TunerParams.amPortSel = (_this->rsp2_antennaPort == 2) ? sdrplay_api_Rsp2_AMPORT_1 : sdrplay_api_Rsp2_AMPORT_2;
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_RfNotchControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_BiasTControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_AntennaControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_AmPortSelect, sdrplay_api_Update_Ext1_None);
        }
        else if (_this->openDev.hwVer == SDRPLAY_RSPduo_ID) {
            
        }
        else if (_this->openDev.hwVer == SDRPLAY_RSPdx_ID) {
            _this->openDevParams->devParams->rspDxParams.rfNotchEnable = _this->rspdx_fmNotch;
            _this->openDevParams->devParams->rspDxParams.rfDabNotchEnable = _this->rspdx_dabNotch;
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
            config.aquire();
            config.conf["device"] = _this->devNameList[_this->devId];
            config.release(true);
        }


        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##sdrplay_sr", _this->name), &_this->srId, sampleRatesTxt)) {
            _this->sampleRate = sampleRates[_this->srId];
            if (_this->bandwidthId == 8) {
                _this->bandwidth = preferedBandwidth[_this->srId];
            }
            core::setInputSampleRate(_this->sampleRate);
            config.aquire();
            config.conf["devices"][_this->selectedName]["sampleRate"] = _this->sampleRate;
            config.release(true);
        }

        if (_this->running) { style::endDisabled(); } 

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##sdrplay_bw", _this->name), &_this->bandwidthId, bandwidthsTxt)) {
            _this->bandwidth = (_this->bandwidthId == 8) ? preferedBandwidth[_this->srId] : bandwidths[_this->bandwidthId];
            if (_this->running) {
                _this->openDevParams->rxChannelA->tunerParams.bwType = _this->bandwidth;
                sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_BwType, sdrplay_api_Update_Ext1_None);
            }
            config.aquire();
            config.conf["devices"][_this->selectedName]["bwMode"] = _this->bandwidthId;
            config.release(true);
        }

        if (_this->deviceOpen) {
            ImGui::PushItemWidth(menuWidth - ImGui::CalcTextSize("LNA Gain").x - 10);
            ImGui::Text("LNA Gain");
            ImGui::SameLine();
            float pos = ImGui::GetCursorPosX();
            if (ImGui::SliderInt(CONCAT("##sdrplay_lna_gain", _this->name), &_this->lnaGain, _this->lnaSteps - 1, 0, "")) {
                _this->openDevParams->rxChannelA->tunerParams.gain.LNAstate = _this->lnaGain;
                sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
                config.aquire();
                config.conf["devices"][_this->selectedName]["lnaGain"] = _this->lnaGain;
                config.release(true);
            }

            if (_this->agc > 0) { style::beginDisabled(); }
            ImGui::Text("IF Gain");
            ImGui::SameLine();
            ImGui::SetCursorPosX(pos);
            if (ImGui::SliderInt(CONCAT("##sdrplay_gain", _this->name), &_this->gain, 59, 20, "")) {
                _this->openDevParams->rxChannelA->tunerParams.gain.gRdB = _this->gain;
                sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
                config.aquire();
                config.conf["devices"][_this->selectedName]["ifGain"] = _this->gain;
                config.release(true);
            }
            ImGui::PopItemWidth();
            if (_this->agc > 0) { style::endDisabled(); }

            ImGui::Text("AGC");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::Combo(CONCAT("##sdrplay_agc", _this->name), &_this->agc, agcModesTxt)) {
                _this->openDevParams->rxChannelA->ctrlParams.agc.enable = agcModes[_this->agc];
                sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Ctrl_Agc, sdrplay_api_Update_Ext1_None);
                if (_this->agc == 0) {
                    _this->openDevParams->rxChannelA->tunerParams.gain.gRdB = _this->gain;
                    sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
                }
                config.aquire();
                config.conf["devices"][_this->selectedName]["agc"] = _this->agc;
                config.release(true);
            }
            

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
        // No options?
    }

    void RSP1AMenu(float menuWidth) {
        if (ImGui::Checkbox(CONCAT("FM Notch##sdrplay_rsp1a_fmnotch", name), &rsp1a_fmNotch)) {
            openDevParams->devParams->rsp1aParams.rfNotchEnable = rsp1a_fmNotch;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp1a_RfNotchControl, sdrplay_api_Update_Ext1_None);
            config.aquire();
            config.conf["devices"][selectedName]["fmNotch"] = rsp1a_fmNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("DAB Notch##sdrplay_rsp1a_dabnotch", name), &rsp1a_dabNotch)) {
            openDevParams->devParams->rsp1aParams.rfNotchEnable = rsp1a_dabNotch;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp1a_RfDabNotchControl, sdrplay_api_Update_Ext1_None);
            config.aquire();
            config.conf["devices"][selectedName]["dabNotch"] = rsp1a_dabNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("Bias-T##sdrplay_rsp1a_biast", name), &rsp1a_biasT)) {
            openDevParams->rxChannelA->rsp1aTunerParams.biasTEnable = rsp1a_biasT;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp1a_BiasTControl, sdrplay_api_Update_Ext1_None);
            config.aquire();
            config.conf["devices"][selectedName]["biast"] = rsp1a_biasT;
            config.release(true);
        }
    }

    void RSP2Menu(float menuWidth) {
        ImGui::Text("Antenna");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##sdrplay_rsp2_ant", name), &rsp2_antennaPort, rsp2_antennaPortsTxt)) {
            openDevParams->rxChannelA->rsp2TunerParams.antennaSel = rsp2_antennaPorts[rsp2_antennaPort];
            openDevParams->rxChannelA->rsp2TunerParams.amPortSel = (rsp2_antennaPort == 2) ? sdrplay_api_Rsp2_AMPORT_1 : sdrplay_api_Rsp2_AMPORT_2;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_AntennaControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_AmPortSelect, sdrplay_api_Update_Ext1_None);
            config.aquire();
            config.conf["devices"][selectedName]["antenna"] = rsp2_antennaPort;
            config.release(true);
        }

        if (ImGui::Checkbox(CONCAT("MW/FM Notch##sdrplay_rsp2_notch", name), &rsp2_notch)) {
            openDevParams->rxChannelA->rsp2TunerParams.rfNotchEnable = rsp2_notch;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_RfNotchControl, sdrplay_api_Update_Ext1_None);
            config.aquire();
            config.conf["devices"][selectedName]["notch"] = rsp2_notch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("Bias-T##sdrplay_rsp2_biast", name), &rsp2_biasT)) {
            openDevParams->rxChannelA->rsp2TunerParams.biasTEnable = rsp2_biasT;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_BiasTControl, sdrplay_api_Update_Ext1_None);
            config.aquire();
            config.conf["devices"][selectedName]["biast"] = rsp2_biasT;
            config.release(true);
        }
    }

    void RSPduoMenu(float menuWidth) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Device currently unsupported");
    }

    void RSPdxMenu(float menuWidth) {
        ImGui::Text("Antenna");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##sdrplay_rspdx_ant", name), &rspdx_antennaPort, rspdx_antennaPortsTxt)) {
            openDevParams->devParams->rspDxParams.antennaSel = rspdx_antennaPorts[rspdx_antennaPort];
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_AntennaControl);
            config.aquire();
            config.conf["devices"][selectedName]["antenna"] = rspdx_antennaPort;
            config.release(true);
        }

        if (ImGui::Checkbox(CONCAT("FM Notch##sdrplay_rspdx_fmnotch", name), &rspdx_fmNotch)) {
            openDevParams->devParams->rspDxParams.rfNotchEnable = rspdx_fmNotch;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfNotchControl);
            config.aquire();
            config.conf["devices"][selectedName]["fmNotch"] = rspdx_fmNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("DAB Notch##sdrplay_rspdx_dabnotch", name), &rspdx_dabNotch)) {
            openDevParams->devParams->rspDxParams.rfDabNotchEnable = rspdx_dabNotch;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfDabNotchControl);
            config.aquire();
            config.conf["devices"][selectedName]["dabNotch"] = rspdx_dabNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("Bias-T##sdrplay_rspdx_biast", name), &rspdx_biasT)) {
            openDevParams->devParams->rspDxParams.biasTEnable = rspdx_biasT;
            sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_BiasTControl);
            config.aquire();
            config.conf["devices"][selectedName]["biast"] = rspdx_biasT;
            config.release(true);
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
            _this->stream.writeBuf[id].re = (float)xi[i] / 32768.0f;
            _this->stream.writeBuf[id].im = (float)xq[i] / 32768.0f;

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
    int lnaSteps = 9;
    int agc = 0;

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

    // RSP Duo Options
    bool rspduo_fmNotch = false;
    bool rspduo_dabNotch = false;
    bool rspduo_biasT = false;

    // RSPdx Options
    bool rspdx_fmNotch = false;
    bool rspdx_dabNotch = false;
    bool rspdx_biasT = false;
    int rspdx_antennaPort = 0;

    std::vector<sdrplay_api_DeviceT> devList;
    std::string devListTxt;
    std::vector<std::string> devNameList;
    std::string selectedName;
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