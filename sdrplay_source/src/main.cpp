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

struct ifMode_t {
    sdrplay_api_If_kHzT ifValue;
    sdrplay_api_Bw_MHzT bw;
    unsigned int deviceSamplerate;
    unsigned int effectiveSamplerate;
};

ifMode_t ifModes[] = {
    { sdrplay_api_IF_Zero,  sdrplay_api_BW_1_536, 2000000, 2000000},
    { sdrplay_api_IF_2_048, sdrplay_api_BW_1_536, 8000000, 2000000},
    { sdrplay_api_IF_2_048, sdrplay_api_BW_5_000, 8000000, 4000000},
    { sdrplay_api_IF_1_620, sdrplay_api_BW_1_536, 6000000, 2000000},
    { sdrplay_api_IF_0_450, sdrplay_api_BW_0_600, 2000000, 1000000},
    { sdrplay_api_IF_0_450, sdrplay_api_BW_0_300, 2000000, 500000},
    { sdrplay_api_IF_0_450, sdrplay_api_BW_0_200, 2000000, 500000},
};

const char* ifModeTxt =
    "ZeroIF\0"
    "IF 2048KHz, IFBW 1536KHz\0"
    "IF 2048KHz, IFBW 5000KHz\0"
    "IF 1620KHz, IFBW 1536KHz\0"
    "IF 450KHz, IFBW 600KHz\0"
    "IF 450KHz, IFBW 300KHz\0"
    "IF 450KHz, IFBW 200KHz\0";

const char* rspduo_antennaPortsTxt = "Tuner 1 (50Ohm)\0Tuner 1 (Hi-Z)\0Tuner 2 (50Ohm)\0";

const char* agcModesTxt = "Off\0005Hz\00050Hz\000100Hz";

class SDRPlaySourceModule : public ModuleManager::Instance {
public:
    SDRPlaySourceModule(std::string name) {
        this->name = name;

        // Init callbacks
        cbFuncs.EventCbFn = eventCB;
        cbFuncs.StreamACbFn = streamCB;
        cbFuncs.StreamBCbFn = streamCB;

        sdrplay_api_ErrT err = sdrplay_api_Open();
        if (err != sdrplay_api_Success) {
            spdlog::error("Could not intiatialized the SDRplay API. Make sure that the service is running.");
            return;
        }

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

        config.acquire();
        std::string confSelectDev = config.conf["device"];
        config.release();
        selectByName(confSelectDev);

        // if (sampleRateList.size() > 0) {
        //     sampleRate = sampleRateList[0];
        // }

        // Select device from config
        // config.acquire();
        // std::string devSerial = config.conf["device"];
        // config.release();
        // selectByString(devSerial);
        // core::setInputSampleRate(sampleRate);

        sigpath::sourceManager.registerSource("SDRplay", &handler);

        initOk = true;
    }

    ~SDRPlaySourceModule() {
        stop(this);
        if (initOk) { sdrplay_api_Close(); }
        sigpath::sourceManager.unregisterSource("SDRplay");
    }

    void postInit() {}

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
        if (devList.size() == 0) {
            selectedName = "";
            return;
        }
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

        openDev.tuner = sdrplay_api_Tuner_A;
        openDev.rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
        err = sdrplay_api_SelectDevice(&openDev);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not select RSP device: {0}", errStr);
            selectedName = "";
            return;
        }

        sdrplay_api_UnlockDeviceApi();
        sdrplay_api_DebugEnable(openDev.dev, sdrplay_api_DbgLvl_Message);

        err = sdrplay_api_GetDeviceParams(openDev.dev, &openDevParams);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not get device params for RSP device: {0}", errStr);
            selectedName = "";
            return;
        }

        err = sdrplay_api_Init(openDev.dev, &cbFuncs, this);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not init RSP device: {0}", errStr);
            selectedName = "";
            return;
        }

        channelParams = openDevParams->rxChannelA;

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
        config.acquire();
        if (!config.conf["devices"].contains(selectedName)) {
            created = true;
            config.conf["devices"][selectedName]["sampleRate"] = sampleRates[0];
            config.conf["devices"][selectedName]["bwMode"] = 8; // Auto
            config.conf["devices"][selectedName]["lnaGain"] = lnaSteps - 1;
            config.conf["devices"][selectedName]["ifGain"] = 59;
            config.conf["devices"][selectedName]["agc"] = 0; // Disabled

            config.conf["devices"][selectedName]["ifModeId"] = 0;

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
                config.conf["devices"][selectedName]["antenna"] = 0;
                config.conf["devices"][selectedName]["fmNotch"] = false;
                config.conf["devices"][selectedName]["dabNotch"] = false;
                config.conf["devices"][selectedName]["amNotch"] = false;
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

        if(config.conf["devices"][selectedName].contains("ifModeId")) {
            ifModeId=config.conf["devices"][selectedName]["ifModeId"];
            if(ifModeId != 0){
                sampleRate = ifModes[ifModeId].effectiveSamplerate;
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

        core::setInputSampleRate(sampleRate);

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
            if (config.conf["devices"][selectedName].contains("antenna")) {
                rspduo_antennaPort = config.conf["devices"][selectedName]["antenna"];
            }
            if (config.conf["devices"][selectedName].contains("fmNotch")) {
                rspduo_fmNotch = config.conf["devices"][selectedName]["fmNotch"];
            }
            if (config.conf["devices"][selectedName].contains("dabNotch")) {
                rspduo_dabNotch = config.conf["devices"][selectedName]["dabNotch"];
            }
            if (config.conf["devices"][selectedName].contains("amNotch")) {
                rspduo_amNotch = config.conf["devices"][selectedName]["amNotch"];
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

        // Release device after selecting
        sdrplay_api_Uninit(openDev.dev);
        sdrplay_api_ReleaseDevice(&openDev);
    }
    
    void rspDuoSelectTuner(sdrplay_api_TunerSelectT tuner, sdrplay_api_RspDuo_AmPortSelectT amPort) {
        if (openDev.tuner != tuner) {
            spdlog::info("Swapping tuners");
            auto ret = sdrplay_api_SwapRspDuoActiveTuner(openDev.dev, &openDev.tuner, amPort);
            if (ret != 0) {
                spdlog::error("Error while swapping tuners: {0}", (int)ret);
            }
        }

        // Change the channel params
        channelParams = (tuner == sdrplay_api_Tuner_A) ? openDevParams->rxChannelA : openDevParams->rxChannelB;
        channelParams->rspDuoTunerParams.tuner1AmPortSel = amPort;
        sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_RspDuo_AmPortSelect, sdrplay_api_Update_Ext1_None);

        // Refresh gains (for some reason they're lost)
        channelParams->tunerParams.gain.LNAstate = lnaGain;
        channelParams->tunerParams.gain.gRdB = gain;
        sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
    
    }

    void rspDuoSelectAntennaPort(int port) {
        if (port == 0) { rspDuoSelectTuner(sdrplay_api_Tuner_A, sdrplay_api_RspDuo_AMPORT_2); }
        if (port == 1) { rspDuoSelectTuner(sdrplay_api_Tuner_A, sdrplay_api_RspDuo_AMPORT_1); }
        if (port == 2) { rspDuoSelectTuner(sdrplay_api_Tuner_B, sdrplay_api_RspDuo_AMPORT_1); }
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
        if (_this->running) { return; }

        // First, acquire device
        sdrplay_api_ErrT err;

        _this->openDev.tuner = sdrplay_api_Tuner_A;
        _this->openDev.rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
        err = sdrplay_api_SelectDevice(&_this->openDev);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not select RSP device: {0}", errStr);
            _this->selectedName = "";
            return;
        }

        sdrplay_api_UnlockDeviceApi();
        sdrplay_api_DebugEnable(_this->openDev.dev, sdrplay_api_DbgLvl_Message);

        err = sdrplay_api_GetDeviceParams(_this->openDev.dev, &_this->openDevParams);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not get device params for RSP device: {0}", errStr);
            _this->selectedName = "";
            return;
        }

        err = sdrplay_api_Init(_this->openDev.dev, &_this->cbFuncs, _this);
        if (err != sdrplay_api_Success) {
            const char* errStr = sdrplay_api_GetErrorString(err);
            spdlog::error("Could not init RSP device: {0}", errStr);
            _this->selectedName = "";
            return;
        }

        _this->channelParams = _this->openDevParams->rxChannelA;

        // Configure device
        _this->bufferIndex = 0;
        _this->bufferSize = (float)_this->sampleRate / 200.0f;

        // RSP1A Options
        if (_this->openDev.hwVer == SDRPLAY_RSP1A_ID) {
            _this->openDevParams->devParams->rsp1aParams.rfNotchEnable = _this->rsp1a_fmNotch;
            _this->openDevParams->devParams->rsp1aParams.rfNotchEnable = _this->rsp1a_dabNotch;
            _this->channelParams->rsp1aTunerParams.biasTEnable = _this->rsp1a_biasT;
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp1a_RfNotchControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp1a_RfDabNotchControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp1a_BiasTControl, sdrplay_api_Update_Ext1_None);
        }
        else if (_this->openDev.hwVer == SDRPLAY_RSP2_ID) {
            _this->channelParams->rsp2TunerParams.rfNotchEnable = _this->rsp2_notch;
            _this->channelParams->rsp2TunerParams.biasTEnable = _this->rsp2_biasT;
            _this->channelParams->rsp2TunerParams.antennaSel = rsp2_antennaPorts[_this->rsp2_antennaPort];
            _this->channelParams->rsp2TunerParams.amPortSel = (_this->rsp2_antennaPort == 2) ? sdrplay_api_Rsp2_AMPORT_1 : sdrplay_api_Rsp2_AMPORT_2;
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_RfNotchControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_BiasTControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_AntennaControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Rsp2_AmPortSelect, sdrplay_api_Update_Ext1_None);
        }
        else if (_this->openDev.hwVer == SDRPLAY_RSPduo_ID) {
            // NOTE: mmight require setting it on both RXA and RXB
            _this->rspDuoSelectAntennaPort(_this->rspduo_antennaPort);
            _this->channelParams->rspDuoTunerParams.biasTEnable = _this->rspduo_biasT;
            _this->channelParams->rspDuoTunerParams.rfNotchEnable = _this->rspduo_fmNotch;
            _this->channelParams->rspDuoTunerParams.rfDabNotchEnable = _this->rspduo_dabNotch;
            _this->channelParams->rspDuoTunerParams.tuner1AmNotchEnable = _this->rspduo_amNotch;
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_RspDuo_BiasTControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_RspDuo_RfNotchControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_RspDuo_RfDabNotchControl, sdrplay_api_Update_Ext1_None);
            sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_RspDuo_Tuner1AmNotchControl, sdrplay_api_Update_Ext1_None);
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

        // General options
        if(_this->ifModeId == 0){
            _this->bandwidth = (_this->bandwidthId == 8) ? preferedBandwidth[_this->srId] : bandwidths[_this->bandwidthId];
            _this->openDevParams->devParams->fsFreq.fsHz = _this->sampleRate;
            _this->channelParams->tunerParams.bwType = _this->bandwidth;
        } else {
            _this->openDevParams->devParams->fsFreq.fsHz = ifModes[_this->ifModeId].deviceSamplerate;
            _this->channelParams->tunerParams.bwType = ifModes[_this->ifModeId].bw;
        }
        _this->channelParams->tunerParams.rfFreq.rfHz = _this->freq;
        _this->channelParams->tunerParams.gain.gRdB = _this->gain;
        _this->channelParams->tunerParams.gain.LNAstate = _this->lnaGain;
        _this->channelParams->ctrlParams.decimation.enable = false;
        _this->channelParams->ctrlParams.dcOffset.DCenable = true;
        _this->channelParams->ctrlParams.dcOffset.IQenable = true;
        _this->channelParams->tunerParams.ifType = ifModes[_this->ifModeId].ifValue;
        _this->channelParams->tunerParams.loMode = sdrplay_api_LO_Auto;

        // Hard coded AGC parameters
        _this->channelParams->ctrlParams.agc.attack_ms = 500;
        _this->channelParams->ctrlParams.agc.decay_ms = 500;
        _this->channelParams->ctrlParams.agc.decay_delay_ms = 200;
        _this->channelParams->ctrlParams.agc.decay_threshold_dB = 5;
        _this->channelParams->ctrlParams.agc.setPoint_dBfs = -30;
        _this->channelParams->ctrlParams.agc.enable = agcModes[_this->agc];

        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Dev_Fs, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_BwType, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_IfType, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_LoMode, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Ctrl_Decimation, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Ctrl_DCoffsetIQimbalance, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Ctrl_Agc, sdrplay_api_Update_Ext1_None);

        _this->running = true;
        spdlog::info("SDRPlaySourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();
        
        // Release device after stopping
        sdrplay_api_Uninit(_this->openDev.dev);
        sdrplay_api_ReleaseDevice(&_this->openDev);

        _this->stream.clearWriteStop();
        spdlog::info("SDRPlaySourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        SDRPlaySourceModule* _this = (SDRPlaySourceModule*)ctx;
        if (_this->running) {
            _this->channelParams->tunerParams.rfFreq.rfHz = freq;
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
            config.acquire();
            config.conf["device"] = _this->devNameList[_this->devId];
            config.release(true);
        }
        
        ImGui::Text("IF Mode");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##sdrplay_ifmode", _this->name), &_this->ifModeId, ifModeTxt)) {
            if(_this->ifModeId != 0){
                _this->bandwidth = ifModes[_this->ifModeId].bw;
                _this->sampleRate = ifModes[_this->ifModeId].effectiveSamplerate;
            } else {
                config.acquire();
                _this->sampleRate = config.conf["devices"][_this->selectedName]["sampleRate"];
                _this->bandwidthId = config.conf["devices"][_this->selectedName]["bwMode"];
                config.release(false);
                _this->bandwidth = (_this->bandwidthId == 8) ? preferedBandwidth[_this->srId] : bandwidths[_this->bandwidthId];
            }            
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["devices"][_this->selectedName]["ifModeId"] = _this->ifModeId;
            config.release(true);
        }

        if (_this->ifModeId == 0) {
            if (ImGui::Combo(CONCAT("##sdrplay_sr", _this->name), &_this->srId, sampleRatesTxt)) {
                _this->sampleRate = sampleRates[_this->srId];
                if (_this->bandwidthId == 8) {
                    _this->bandwidth = preferedBandwidth[_this->srId];
                }
                core::setInputSampleRate(_this->sampleRate);
                config.acquire();
                config.conf["devices"][_this->selectedName]["sampleRate"] = _this->sampleRate;
                config.release(true);
            }

            ImGui::SameLine();
            float refreshBtnWdith = menuWidth - ImGui::GetCursorPosX();
            if (ImGui::Button(CONCAT("Refresh##sdrplay_refresh", _this->name), ImVec2(refreshBtnWdith, 0))) {
                _this->refresh();
                _this->selectByName(_this->selectedName);
            }

            ImGui::SetNextItemWidth(menuWidth);
            if (ImGui::Combo(CONCAT("##sdrplay_bw", _this->name), &_this->bandwidthId, bandwidthsTxt)) {
                _this->bandwidth = (_this->bandwidthId == 8) ? preferedBandwidth[_this->srId] : bandwidths[_this->bandwidthId];
                if (_this->running) {
                    _this->channelParams->tunerParams.bwType = _this->bandwidth;
                    sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_BwType, sdrplay_api_Update_Ext1_None);
                }
                config.acquire();
                config.conf["devices"][_this->selectedName]["bwMode"] = _this->bandwidthId;
                config.release(true);
            }
        }

        if (_this->running) { style::endDisabled(); } 

        if (_this->selectedName != "") {
            ImGui::PushItemWidth(menuWidth - ImGui::CalcTextSize("LNA Gain").x - 10);
            ImGui::Text("LNA Gain");
            ImGui::SameLine();
            float pos = ImGui::GetCursorPosX();
            if (ImGui::SliderInt(CONCAT("##sdrplay_lna_gain", _this->name), &_this->lnaGain, _this->lnaSteps - 1, 0, "")) {
                if (_this->running) {
                    _this->channelParams->tunerParams.gain.LNAstate = _this->lnaGain;
                    sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
                }
                config.acquire();
                config.conf["devices"][_this->selectedName]["lnaGain"] = _this->lnaGain;
                config.release(true);
            }

            if (_this->agc > 0) { style::beginDisabled(); }
            ImGui::Text("IF Gain");
            ImGui::SameLine();
            ImGui::SetCursorPosX(pos);
            if (ImGui::SliderInt(CONCAT("##sdrplay_gain", _this->name), &_this->gain, 59, 20, "")) {
                if (_this->running) {
                    _this->channelParams->tunerParams.gain.gRdB = _this->gain;
                    sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
                }
                config.acquire();
                config.conf["devices"][_this->selectedName]["ifGain"] = _this->gain;
                config.release(true);
            }
            ImGui::PopItemWidth();
            if (_this->agc > 0) { style::endDisabled(); }

            ImGui::Text("AGC");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::Combo(CONCAT("##sdrplay_agc", _this->name), &_this->agc, agcModesTxt)) {
                if (_this->running) {
                    _this->channelParams->ctrlParams.agc.enable = agcModes[_this->agc];
                    sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Ctrl_Agc, sdrplay_api_Update_Ext1_None);
                    if (_this->agc == 0) {
                        _this->channelParams->tunerParams.gain.gRdB = _this->gain;
                        sdrplay_api_Update(_this->openDev.dev, _this->openDev.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
                    }
                }
                config.acquire();
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
            if (running) {
                openDevParams->devParams->rsp1aParams.rfNotchEnable = rsp1a_fmNotch;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp1a_RfNotchControl, sdrplay_api_Update_Ext1_None);
            }
            config.acquire();
            config.conf["devices"][selectedName]["fmNotch"] = rsp1a_fmNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("DAB Notch##sdrplay_rsp1a_dabnotch", name), &rsp1a_dabNotch)) {
            if (running) {
                openDevParams->devParams->rsp1aParams.rfNotchEnable = rsp1a_dabNotch;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp1a_RfDabNotchControl, sdrplay_api_Update_Ext1_None);
            }
            config.acquire();
            config.conf["devices"][selectedName]["dabNotch"] = rsp1a_dabNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("Bias-T##sdrplay_rsp1a_biast", name), &rsp1a_biasT)) {
            if (running) {
                channelParams->rsp1aTunerParams.biasTEnable = rsp1a_biasT;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp1a_BiasTControl, sdrplay_api_Update_Ext1_None);
            }
            config.acquire();
            config.conf["devices"][selectedName]["biast"] = rsp1a_biasT;
            config.release(true);
        }
    }

    void RSP2Menu(float menuWidth) {
        ImGui::Text("Antenna");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##sdrplay_rsp2_ant", name), &rsp2_antennaPort, rsp2_antennaPortsTxt)) {
            if (running) {
                channelParams->rsp2TunerParams.antennaSel = rsp2_antennaPorts[rsp2_antennaPort];
                channelParams->rsp2TunerParams.amPortSel = (rsp2_antennaPort == 2) ? sdrplay_api_Rsp2_AMPORT_1 : sdrplay_api_Rsp2_AMPORT_2;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_AntennaControl, sdrplay_api_Update_Ext1_None);
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_AmPortSelect, sdrplay_api_Update_Ext1_None);
            }
            config.acquire();
            config.conf["devices"][selectedName]["antenna"] = rsp2_antennaPort;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("MW/FM Notch##sdrplay_rsp2_notch", name), &rsp2_notch)) {
            if (running) {
                channelParams->rsp2TunerParams.rfNotchEnable = rsp2_notch;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_RfNotchControl, sdrplay_api_Update_Ext1_None);
            }
            config.acquire();
            config.conf["devices"][selectedName]["notch"] = rsp2_notch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("Bias-T##sdrplay_rsp2_biast", name), &rsp2_biasT)) {
            if (running) {
                channelParams->rsp2TunerParams.biasTEnable = rsp2_biasT;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_Rsp2_BiasTControl, sdrplay_api_Update_Ext1_None);
            }
            config.acquire();
            config.conf["devices"][selectedName]["biast"] = rsp2_biasT;
            config.release(true);
        }
    }

    void RSPduoMenu(float menuWidth) {
        ImGui::Text("Antenna");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());

        if (ImGui::Combo(CONCAT("##sdrplay_rspduo_ant", name), &rspduo_antennaPort, rspduo_antennaPortsTxt)) {
            if (running) {
                rspDuoSelectAntennaPort(rspduo_antennaPort);
            }
            config.acquire();
            config.conf["devices"][selectedName]["antenna"] = rspduo_antennaPort;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("FM Notch##sdrplay_rspduo_notch", name), &rspduo_fmNotch)) {
            if (running) {
                channelParams->rspDuoTunerParams.rfNotchEnable = rspduo_fmNotch;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_RspDuo_RfNotchControl, sdrplay_api_Update_Ext1_None);
            }
            config.acquire();
            config.conf["devices"][selectedName]["fmNotch"] = rspduo_fmNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("DAB Notch##sdrplay_rspduo_dabnotch", name), &rspduo_dabNotch)) {
            if (running) {
                channelParams->rspDuoTunerParams.rfDabNotchEnable = rspduo_dabNotch;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_RspDuo_RfDabNotchControl, sdrplay_api_Update_Ext1_None);
            }
            config.acquire();
            config.conf["devices"][selectedName]["dabNotch"] = rspduo_dabNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("AM Notch##sdrplay_rspduo_dabnotch", name), &rspduo_amNotch)) {
            if (running) {
                channelParams->rspDuoTunerParams.tuner1AmNotchEnable = rspduo_amNotch;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_RspDuo_Tuner1AmNotchControl, sdrplay_api_Update_Ext1_None);
            }
            config.acquire();
            config.conf["devices"][selectedName]["amNotch"] = rspduo_amNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("Bias-T##sdrplay_rspduo_biast", name), &rspduo_biasT)) {
            if (running) {
                channelParams->rspDuoTunerParams.biasTEnable = rspduo_biasT;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_RspDuo_BiasTControl, sdrplay_api_Update_Ext1_None);
            }
            config.acquire();
            config.conf["devices"][selectedName]["biast"] = rspduo_biasT;
            config.release(true);
        }
    }

    void RSPdxMenu(float menuWidth) {
        ImGui::Text("Antenna");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##sdrplay_rspdx_ant", name), &rspdx_antennaPort, rspdx_antennaPortsTxt)) {
            if (running) {
                openDevParams->devParams->rspDxParams.antennaSel = rspdx_antennaPorts[rspdx_antennaPort];
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_AntennaControl);
            }
            config.acquire();
            config.conf["devices"][selectedName]["antenna"] = rspdx_antennaPort;
            config.release(true);
        }

        if (ImGui::Checkbox(CONCAT("FM Notch##sdrplay_rspdx_fmnotch", name), &rspdx_fmNotch)) {
            if (running) {
                openDevParams->devParams->rspDxParams.rfNotchEnable = rspdx_fmNotch;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfNotchControl);
            }
            config.acquire();
            config.conf["devices"][selectedName]["fmNotch"] = rspdx_fmNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("DAB Notch##sdrplay_rspdx_dabnotch", name), &rspdx_dabNotch)) {
            if (running) {
                openDevParams->devParams->rspDxParams.rfDabNotchEnable = rspdx_dabNotch;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfDabNotchControl);
            }
            config.acquire();
            config.conf["devices"][selectedName]["dabNotch"] = rspdx_dabNotch;
            config.release(true);
        }
        if (ImGui::Checkbox(CONCAT("Bias-T##sdrplay_rspdx_biast", name), &rspdx_biasT)) {
            if (running) {
                openDevParams->devParams->rspDxParams.biasTEnable = rspdx_biasT;
                sdrplay_api_Update(openDev.dev, openDev.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_BiasTControl);
            }
            config.acquire();
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
        // TODO: Optimise using volk and math
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
    bool initOk = false;

    sdrplay_api_CallbackFnsT cbFuncs;

    sdrplay_api_DeviceT openDev;
    sdrplay_api_DeviceParamsT * openDevParams;
    sdrplay_api_RxChannelParamsT* channelParams;

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

    int ifModeId = 0;

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
    bool rspduo_amNotch = false;
    int rspduo_antennaPort = 0;

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
