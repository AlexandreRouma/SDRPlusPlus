#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <options.h>
#include <gui/widgets/stepped_slider.h>
#include <libbladeRF.h>
#include <dsp/processing.h>
#include <algorithm>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

#define NUM_BUFFERS     128
#define NUM_TRANSFERS   1

SDRPP_MOD_INFO {
    /* Name:            */ "bladerf_source",
    /* Description:     */ "BladeRF source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};


ConfigManager config;

class BladeRFSourceModule : public ModuleManager::Instance {
public:
    BladeRFSourceModule(std::string name) {
        this->name = name;

        sampleRate = 1000000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        refresh();

        // Select device here
        config.aquire();
        std::string serial = config.conf["device"];
        config.release();
        selectBySerial(serial);
        core::setInputSampleRate(sampleRate);

        sigpath::sourceManager.registerSource("BladeRF", &handler);
    }

    ~BladeRFSourceModule() {
        
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
        devListTxt = "";

        if (devInfoList != NULL) {
            bladerf_free_device_list(devInfoList);
        }

        devCount = bladerf_get_device_list(&devInfoList);
        if (devCount < 0) {
            spdlog::error("Could not list devices");
            return;
        }
        for (int i = 0; i < devCount; i++) {
            // Keep only the first 32 character of the serial number for display
            devListTxt += std::string(devInfoList[i].serial).substr(0, 16);
            devListTxt += '\0';
        }
    }

    void selectFirst() {
        if (devCount > 0) { selectByInfo(&devInfoList[0]); }
    }

    void selectBySerial(std::string serial, bool reloadChannelId = true) {
        if (serial == "") { 
            selectFirst();
            return;
        }
        for (int i = 0; i < devCount; i++) {
            bladerf_devinfo info = devInfoList[i];
            if (serial == info.serial) {
                devId = i;
                selectByInfo(&info, reloadChannelId);
                return;
            }
        }
    }

    void selectByInfo(bladerf_devinfo* info, bool reloadChannelId = true) {
        int ret = bladerf_open_with_devinfo(&openDev, info);
        if (ret != 0) {
            spdlog::error("Could not open device {0}", info->serial);
            selectedSerial = "";
            return;
        }

        selectedSerial = info->serial;
        for (int i = 0; i < devCount; i++) {
            if (selectedSerial == devInfoList[i].serial) { devId = i; }
        }

        // Gather info about the BladeRF's ranges
        channelCount = bladerf_get_channel_count(openDev, BLADERF_RX);

        // Load the channelId if there are more than 1 channel
        if (reloadChannelId) {
            config.aquire();
            if (channelCount > 1 && config.conf["devices"].contains(info->serial)) {
                if (config.conf["devices"][info->serial].contains("channelId")) {
                    chanId = config.conf["devices"][info->serial]["channelId"];
                }
                else { chanId = 0; }
            }
            else { chanId = 0; }
            config.release();
        }

        chanId = std::clamp<int>(chanId, 0, channelCount - 1);

        bladerf_get_sample_rate_range(openDev, BLADERF_CHANNEL_RX(chanId), &srRange);
        bladerf_get_bandwidth_range(openDev, BLADERF_CHANNEL_RX(chanId), &bwRange);
        bladerf_get_gain_range(openDev, BLADERF_CHANNEL_RX(chanId), &gainRange);

        // Generate sampleRate and Bandwidth lists
        sampleRates.clear();
        sampleRatesTxt = "";
        sampleRates.push_back(srRange->min);
        sampleRatesTxt += getBandwdithScaled(srRange->min);
        sampleRatesTxt += '\0';
        for (int i = 2000000; i < srRange->max; i += 2000000) {
            sampleRates.push_back(i);
            sampleRatesTxt += getBandwdithScaled(i);
            sampleRatesTxt += '\0';
        }
        sampleRates.push_back(srRange->max);
        sampleRatesTxt += getBandwdithScaled(srRange->max);
        sampleRatesTxt += '\0';

        // Generate bandwidth list
        bandwidths.clear();
        bandwidthsTxt = "";
        bandwidths.push_back(bwRange->min);
        bandwidthsTxt += getBandwdithScaled(bwRange->min);
        bandwidthsTxt += '\0';
        for (int i = 2000000; i < bwRange->max; i += 2000000) {
            bandwidths.push_back(i);
            bandwidthsTxt += getBandwdithScaled(i);
            bandwidthsTxt += '\0';
        }
        bandwidths.push_back(bwRange->max);
        bandwidthsTxt += getBandwdithScaled(bwRange->max);
        bandwidthsTxt += '\0';
        bandwidthsTxt += "Auto";
        bandwidthsTxt += '\0';

        // Generate list of channel names
        channelNamesTxt = "";
        char buf[32];
        for (int i = 0; i < channelCount; i++) {
            sprintf(buf, "RX %d", i+1);
            channelNamesTxt += buf;
            channelNamesTxt += '\0';
        }

        // Load settings here

        srId = 0;
        sampleRate = sampleRates[0];

        // Set bandwidth to auto as default
        bwId = bandwidths.size();

        bladerf_close(openDev);
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
        BladeRFSourceModule* _this = (BladeRFSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("BladeRFSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        BladeRFSourceModule* _this = (BladeRFSourceModule*)ctx;
        spdlog::info("BladeRFSourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        BladeRFSourceModule* _this = (BladeRFSourceModule*)ctx;
        if (_this->running) {
            return;
        }
        if (_this->devCount == 0) { return; }

        // Open device
        bladerf_devinfo info = _this->devInfoList[_this->devId];
        int ret = bladerf_open_with_devinfo(&_this->openDev, &info);
        if (ret != 0) {
            spdlog::error("Could not open device {0}", info.serial);
            return;
        }

        // Calculate buffer size, must be a multiple of 1024
        _this->bufferSize = _this->sampleRate / 200.0;
        _this->bufferSize /= 1024;
        _this->bufferSize *= 1024;
        if (_this->bufferSize < 1024) { _this->bufferSize = 1024; }

        // Setup device parameters
        bladerf_set_sample_rate(_this->openDev, BLADERF_CHANNEL_RX(_this->chanId), _this->sampleRate, NULL);
        bladerf_set_frequency(_this->openDev, BLADERF_CHANNEL_RX(_this->chanId), _this->freq);
        bladerf_set_bandwidth(_this->openDev, BLADERF_CHANNEL_RX(_this->chanId), (_this->bwId == _this->bandwidths.size()) ? 
                            std::clamp<uint64_t>(_this->sampleRate, _this->bwRange->min, _this->bwRange->max) : _this->bandwidths[_this->bwId], NULL);
        bladerf_set_gain_mode(_this->openDev, BLADERF_CHANNEL_RX(_this->chanId), BLADERF_GAIN_MANUAL);
        bladerf_set_gain(_this->openDev, BLADERF_CHANNEL_RX(_this->chanId), _this->testGain);

        // Setup syncronous transfer
        bladerf_sync_config(_this->openDev, BLADERF_RX_X1, BLADERF_FORMAT_SC16_Q11, 16, _this->bufferSize, 8, 3500);

        // Enable streaming
        bladerf_enable_module(_this->openDev, BLADERF_CHANNEL_RX(_this->chanId), true);

        _this->running = true;
        _this->workerThread = std::thread(&BladeRFSourceModule::worker, _this);

        spdlog::info("BladeRFSourceModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        BladeRFSourceModule* _this = (BladeRFSourceModule*)ctx;
        if (!_this->running) {
            return;
        }
        _this->running = false;
        _this->stream.stopWriter();
        
        // Disable streaming
        bladerf_enable_module(_this->openDev, BLADERF_CHANNEL_RX(_this->chanId), false);

        // Wait for read worker to terminate
        if (_this->workerThread.joinable()) {
            _this->workerThread.join();
        }

        // Close device
        bladerf_close(_this->openDev);

        _this->stream.clearWriteStop();
        spdlog::info("BladeRFSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        BladeRFSourceModule* _this = (BladeRFSourceModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            bladerf_set_frequency(_this->openDev, BLADERF_CHANNEL_RX(_this->chanId), _this->freq);
        }
        spdlog::info("BladeRFSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        BladeRFSourceModule* _this = (BladeRFSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_balderf_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            bladerf_devinfo info = _this->devInfoList[_this->devId];
            _this->selectByInfo(&info);
            core::setInputSampleRate(_this->sampleRate);
            config.aquire();
            config.conf["device"] = _this->selectedSerial;
            config.release(true);
        }

        if (ImGui::Combo(CONCAT("##_balderf_sr_sel_", _this->name), &_this->srId, _this->sampleRatesTxt.c_str())) {
            _this->sampleRate = _this->sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            // Save config
        }

        // Refresh button
        ImGui::SameLine();
        float refreshBtnWdith = menuWidth - ImGui::GetCursorPosX();
        if (ImGui::Button(CONCAT("Refresh##_balderf_refr_", _this->name), ImVec2(refreshBtnWdith, 0))) {
            _this->refresh();
            _this->selectBySerial(_this->selectedSerial, false);
            core::setInputSampleRate(_this->sampleRate);
        }

        // Channel selection (only show if more than one channel)


        if (_this->running) { style::endDisabled(); }

        ImGui::Text("Bandwidth");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_balderf_bw_sel_", _this->name), &_this->bwId, _this->bandwidthsTxt.c_str())) {
            if (_this->running) {
                bladerf_set_bandwidth(_this->openDev, BLADERF_CHANNEL_RX(_this->chanId), (_this->bwId == _this->bandwidths.size()) ? 
                            std::clamp<uint64_t>(_this->sampleRate, _this->bwRange->min, _this->bwRange->max) : _this->bandwidths[_this->bwId], NULL);
            }
            // Save config
        }

        // General config BS
        if (ImGui::SliderInt("Test Gain", &_this->testGain, (_this->gainRange != NULL) ? _this->gainRange->min : 0, (_this->gainRange != NULL) ? _this->gainRange->max : 60)) {
            if (_this->running) {
                spdlog::info("Setting gain to {0}", _this->testGain);
                bladerf_set_gain(_this->openDev, BLADERF_CHANNEL_RX(_this->chanId), _this->testGain);
            }
        }

    }

    void worker() {
        int16_t* buffer = new int16_t[bufferSize * 2];
        bladerf_metadata meta;
        
        while (true) {
            // Receive from the stream and break on error
            int ret = bladerf_sync_rx(openDev, buffer, bufferSize, &meta, 3500);
            if (ret != 0) { break; }

            // Convert to complex float and swap buffers
            volk_16i_s32f_convert_32f((float*)stream.writeBuf, buffer, 32768.0f, bufferSize * 2);
            if (!stream.swap(bufferSize)) { break; }
        }

        delete[] buffer;
    }

    std::string name;
    bladerf* openDev;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    int devId = 0;
    int srId = 0;
    int bwId = 0;
    int chanId = 0;

    int channelCount;

    const bladerf_range* srRange = NULL;
    const bladerf_range* bwRange = NULL;
    const bladerf_range* gainRange = NULL;

    std::vector<uint64_t> sampleRates;
    std::string sampleRatesTxt;
    std::vector<uint64_t> bandwidths;
    std::string bandwidthsTxt;

    std::string channelNamesTxt;

    int bufferSize;
    struct bladerf_stream* rxStream;

    int testGain = 0;

    std::thread workerThread;

    int devCount = 0;
    bladerf_devinfo* devInfoList = NULL;
    std::string devListTxt;

    std::string selectedSerial;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(options::opts.root + "/bladerf_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new BladeRFSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (BladeRFSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}