#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <options.h>
#include <lime/LimeSuite.h>


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "limesdr_source",
    /* Description:     */ "LimeSDR source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class LimeSDRSourceModule : public ModuleManager::Instance {
public:
    LimeSDRSourceModule(std::string name) {
        this->name = name;

        // Init limesuite if needed

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

        // Select device from config
        selectFirst();

        sigpath::sourceManager.registerSource("LimeSDR", &handler);
    }

    ~LimeSDRSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("LimeSDR");
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
        devCount = LMS_GetDeviceList(devList);
        char buf[256];
        devListTxt = "";

        for (int i = 0; i < devCount; i++) {
            lms_device_t* dev = NULL;
            LMS_Open(&dev, devList[i], NULL);
            const lms_dev_info_t* info = LMS_GetDeviceInfo(dev);
            sprintf(buf, "%s [%" PRIX64 "]", info->deviceName, info->boardSerialNumber);
            LMS_Close(dev);

            devNames.push_back(buf);
            devListTxt += buf;
            devListTxt += '\0';
        }
    }

    void selectFirst() {
        if (devCount > 0) {
            selectByInfoStr(devList[0]);
            return;
        }
        selectedDevName = "";
    }

    void selectByName(std::string name) {
        for (int i = 0; i < devCount; i++) {
            if (devNames[i] == name) {
                selectByInfoStr(devList[i]);
                break;
            }
        }
        selectFirst();
    }

    void selectByInfoStr(lms_info_str_t info) {
        if (devCount == 0) {
            selectedDevName = "";
            return;
        }

        // Set devId and selectedDevNames
        for (int i = 0; i < devCount; i++) {
            if (info == devList[i]) {
                devId = i;
                selectedDevName = devNames[i];
                break;
            }
        }

        lms_device_t* dev = NULL;
        LMS_Open(&dev, info, NULL);

        channelCount = LMS_GetNumChannels(dev, false);
        char buf[32];
        for (int i = 0; i < channelCount; i++) {
            sprintf(buf, "CH %d", i+1);
            channelNamesTxt += buf;
            channelNamesTxt += '\0';
        }

        config.acquire();
        if (config.conf["devices"].contains(selectedDevName)) {
            if (config.conf["devices"][selectedDevName].contains("channel")) {
                chanId = config.conf["devices"][selectedDevName]["channel"];
            }
            else { chanId = 0; }
        }
        else { chanId = 0; }
        config.release();

        chanId = std::clamp<int>(chanId, 0, channelCount - 1);

        // List antennas
        lms_name_t antennaNames[16];
        antennaCount = LMS_GetAntennaList(dev, false, chanId, antennaNames);
        antennaNameList.clear();
        antennaListTxt = "";
        for (int i = 0; i < antennaCount; i++) {
            antennaNameList.push_back(antennaNames[i]);
            antennaListTxt += antennaNames[i];
            antennaListTxt += '\0';
        }

        // List supported sample rates
        lms_range_t srRange;
        LMS_GetSampleRateRange(dev, false, &srRange);
        sampleRates.clear();
        sampleRatesTxt = "";
        sampleRates.push_back(srRange.min);
        sampleRatesTxt += getBandwdithScaled(srRange.min);
        sampleRatesTxt += '\0';
        for (int i = 1000000; i < srRange.max; i += 1000000) {
            sampleRates.push_back(i);
            sampleRatesTxt += getBandwdithScaled(i);
            sampleRatesTxt += '\0';
        }
        sampleRates.push_back(srRange.max);
        sampleRatesTxt += getBandwdithScaled(srRange.max);
        sampleRatesTxt += '\0';

        // List supported bandwidths
        lms_range_t bwRange;
        LMS_GetLPFBWRange(dev, false, &bwRange);
        bandwidths.clear();
        bandwidthsTxt = "";
        bandwidths.push_back(bwRange.min);
        bandwidthsTxt += getBandwdithScaled(bwRange.min);
        bandwidthsTxt += '\0';
        for (int i = 2000000; i < bwRange.max; i += 1000000) {
            bandwidths.push_back(i);
            bandwidthsTxt += getBandwdithScaled(i);
            bandwidthsTxt += '\0';
        }
        bandwidths.push_back(bwRange.max);
        bandwidthsTxt += getBandwdithScaled(bwRange.max);
        bandwidthsTxt += '\0';
        bandwidthsTxt += "Auto";
        bandwidthsTxt += '\0';

        config.acquire();

        if (!config.conf["devices"].contains(selectedDevName)) {
            config.conf["devices"][selectedDevName]["sampleRate"] = sampleRates[0];
            config.conf["devices"][selectedDevName]["channel"] = 0;
            config.conf["devices"][selectedDevName]["antenna"] = "LNAW";
            config.conf["devices"][selectedDevName]["bandwidth"] = bandwidths.size();
            config.conf["devices"][selectedDevName]["gain"] = 0;
        }

        // Load sample rate
        if (config.conf["devices"][selectedDevName].contains("sampleRate")) {
            bool found = false;
            int sr = config.conf["devices"][selectedDevName]["sampleRate"];
            for (int i = 0; i < sampleRates.size(); i++) {
                if (sr == sampleRates[i]) {
                    srId = i;
                    sampleRate = sampleRates[i];
                    found = true;
                    break;
                }
            }
            if (!found) {
                srId = 0;
                sampleRate = sampleRates[0];
            }
        }
        else {
            srId = 0;
            sampleRate = sampleRates[0];
        }

        // Load antenna
        if (config.conf["devices"][selectedDevName].contains("antenna")) {
            std::string antName = config.conf["devices"][selectedDevName]["antenna"];
            bool found = false;
            for (int i = 0; i < antennaCount; i++) {
                if (antennaNames[i] == antName) {
                    antennaId = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (int i = 0; i < antennaCount; i++) {
                    if (antennaNames[i] == "LNAW") {
                        antennaId = i;
                        found = true;
                        break;
                    }
                }
                if (!found) { antennaId = 0; }
            }
        }
        else {
            bool found = false;
            for (int i = 0; i < antennaCount; i++) {
                if (antennaNames[i] == "LNAW") {
                    antennaId = i;
                    found = true;
                    break;
                }
            }
            if (!found) { antennaId = 0; }
        }

        // Load bandwidth
        if (config.conf["devices"][selectedDevName].contains("bandwidth")) {
            bwId = config.conf["devices"][selectedDevName]["bandwidth"];
            bwId = std::clamp<int>(bwId, 0, bandwidths.size());
        }
        else {
            bwId = bandwidths.size();
        }

        // Load gain
        if (config.conf["devices"][selectedDevName].contains("gain")) {
            gain = config.conf["devices"][selectedDevName]["gain"];
            gain = std::clamp<int>(gain, 0, 73);
        }
        else {
            gain = 0;
        }

        config.release(true);

        LMS_Close(dev);
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

    int getBestBandwidth(int sampleRate) {
        for (int i = 0; i < bandwidths.size(); i++) {
            if (bandwidths[i] >= sampleRate) {
                spdlog::warn("Selected bandwidth is {0}", bandwidths[i]);
                return bandwidths[i];
            }
        }
        return bandwidths[bandwidths.size()-1];
    }

    static void menuSelected(void* ctx) {
        LimeSDRSourceModule* _this = (LimeSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("LimeSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        LimeSDRSourceModule* _this = (LimeSDRSourceModule*)ctx;
        spdlog::info("LimeSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        LimeSDRSourceModule* _this = (LimeSDRSourceModule*)ctx;
        if (_this->running) { return; }
        
        // Open device
        _this->openDev = NULL;
        LMS_Open(&_this->openDev, _this->devList[_this->devId], NULL);
        LMS_Init(_this->openDev);

        spdlog::warn("Channel count: {0}", LMS_GetNumChannels(_this->openDev, false));

        // Set options
        LMS_EnableChannel(_this->openDev, false, _this->chanId, true);
        LMS_SetSampleRate(_this->openDev, _this->sampleRate, 0);
        LMS_SetLOFrequency(_this->openDev, false, _this->chanId, _this->freq);
        LMS_SetGaindB(_this->openDev, false, _this->chanId, _this->gain);
        LMS_SetLPFBW(_this->openDev, false, _this->chanId, (_this->bwId == _this->bandwidths.size()) ? _this->getBestBandwidth(_this->sampleRate) : _this->bandwidths[_this->bwId]);
        LMS_SetLPF(_this->openDev, false, _this->chanId, true);

        // Setup and start stream
        int sampCount = _this->sampleRate / 200;
        _this->devStream.isTx = false;
        _this->devStream.channel = _this->chanId;
        _this->devStream.fifoSize = sampCount; // TODO: Check what it's actually supposed to be
        _this->devStream.throughputVsLatency = 0.5f;
        _this->devStream.dataFmt = _this->devStream.LMS_FMT_F32;
        LMS_SetupStream(_this->openDev, &_this->devStream);

        // Start stream
        _this->streamRunning = true;
        LMS_StartStream(&_this->devStream);
        _this->workerThread = std::thread(&LimeSDRSourceModule::worker, _this);


        _this->running = true;
        spdlog::info("LimeSDRSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        LimeSDRSourceModule* _this = (LimeSDRSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;

        _this->streamRunning = false;
        if (_this->workerThread.joinable()) { _this->workerThread.join(); }

        LMS_StopStream(&_this->devStream);
        LMS_DestroyStream(_this->openDev, &_this->devStream);

        LMS_Close(_this->openDev);

        spdlog::info("LimeSDRSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        LimeSDRSourceModule* _this = (LimeSDRSourceModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            LMS_SetLOFrequency(_this->openDev, false, _this->chanId, freq);
        }
        spdlog::info("LimeSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        LimeSDRSourceModule* _this = (LimeSDRSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo("##limesdr_dev_sel", &_this->devId, _this->devListTxt.c_str())) {
            _this->selectByInfoStr(_this->devList[_this->devId]);
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["device"] = _this->selectedDevName;
            config.release(true);
        }

        if (ImGui::Combo(CONCAT("##_limesdr_sr_sel_", _this->name), &_this->srId, _this->sampleRatesTxt.c_str())) {
            _this->sampleRate = _this->sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["sampleRate"] = _this->sampleRates[_this->srId];
                config.release(true);
            }
        }

        // Refresh button
        ImGui::SameLine();
        float refreshBtnWdith = menuWidth - ImGui::GetCursorPosX();
        if (ImGui::Button(CONCAT("Refresh##_limesdr_refr_", _this->name), ImVec2(refreshBtnWdith, 0))) {
            _this->refresh();
            _this->selectByName(_this->selectedDevName);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->channelCount > 1) {
            ImGui::Text("RX Channel");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::Combo("##limesdr_ch_sel", &_this->chanId, _this->channelNamesTxt.c_str()) && _this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["channel"] = _this->chanId;
                config.release(true);
            }
        }

        if (_this->running) { style::endDisabled(); }

        ImGui::Text("Antenna");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##limesdr_ant_sel", &_this->antennaId, _this->antennaListTxt.c_str())) {
            if (_this->running) {
                LMS_SetAntenna(_this->openDev, false, _this->chanId, _this->antennaId);
            }
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["antenna"] = _this->antennaNameList[_this->antennaId];
                config.release(true);
            }
        }

        ImGui::Text("Bandwidth");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##limesdr_bw_sel", &_this->bwId, _this->bandwidthsTxt.c_str())) {
            if (_this->running) {
                LMS_SetLPFBW(_this->openDev, false, _this->chanId, (_this->bwId == _this->bandwidths.size()) ? _this->getBestBandwidth(_this->sampleRate) : _this->bandwidths[_this->bwId]);
            }
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["bandwidth"] = _this->bwId;
                config.release(true);
            }
        }

        ImGui::Text("Gain");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderInt("##limesdr_gain_sel", &_this->gain, 0, 73, "%ddB")) {
            if (_this->running) {
                LMS_SetGaindB(_this->openDev, false, _this->chanId, _this->gain);
            }
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["gain"] = _this->gain;
                config.release(true);
            }
        }
    }

    void worker() {
        int sampCount = sampleRate / 200;
        lms_stream_meta_t meta;
        while (streamRunning) {
            int ret = LMS_RecvStream(&devStream, stream.writeBuf, sampCount, &meta, 1000);
            if (!stream.swap(sampCount) || ret < 0) { break; }
        }
    }

    std::string name;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    bool enabled = true;
    bool streamRunning = false;
    double freq;

    int channelCount = 0;

    int devId = 0;
    int chanId = 0;
    int srId = 0;
    int bwId = 0;
    int gain = 0;

    std::vector<int> sampleRates;
    std::string sampleRatesTxt;
    std::vector<int> bandwidths;
    std::string bandwidthsTxt;

    lms_info_str_t devList[128];
    int devCount = 0;
    std::string devListTxt;
    std::vector<std::string> devNames;
    std::string selectedDevName;

    lms_device_t* openDev;

    lms_stream_t devStream;

    std::string channelNamesTxt;

    int antennaId = 0;
    std::string antennaListTxt;
    std::vector<std::string> antennaNameList;
    int antennaCount = 0;

    std::thread workerThread;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(options::opts.root + "/limesdr_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new LimeSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (LimeSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}