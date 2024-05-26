#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <gui/widgets/stepped_slider.h>
#include <utils/optionlist.h>
#include <RtAudio.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "audio_source",
    /* Description:     */ "Audio source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

struct DeviceInfo {
    RtAudio::DeviceInfo info;
    int id;
    bool operator==(const struct DeviceInfo& other) const {
        return other.id == id;
    }
};

class AudioSourceModule : public ModuleManager::Instance {
public:
    AudioSourceModule(std::string name) {
        this->name = name;

#if RTAUDIO_VERSION_MAJOR >= 6
        audio.setErrorCallback(&errorCallback);
#endif

        sampleRate = 48000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        // Refresh devices
        refresh();

        // Select device
        std::string device = "";
        config.acquire();
        if (config.conf.contains("device")) {
            device = config.conf["device"];
        }
        config.release();
        select(device);
        
        sigpath::sourceManager.registerSource("Audio", &handler);
    }

    ~AudioSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("Audio");
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
        devices.clear();

#if RTAUDIO_VERSION_MAJOR >= 6
        for (int i : audio.getDeviceIds()) {
#else
        int count = audio.getDeviceCount();
        for (int i = 0; i < count; i++) {
#endif
            try {
                // Get info
                auto info = audio.getDeviceInfo(i);

#if !defined(RTAUDIO_VERSION_MAJOR) || RTAUDIO_VERSION_MAJOR < 6
                if (!info.probed) { continue; }
#endif
                // Check that it has a stereo input
                if (info.inputChannels < 2) { continue; }

                // Save info
                DeviceInfo dinfo = { info, i };
                devices.define(info.name, info.name, dinfo);
            }
            catch (const std::exception& e) {
                flog::error("Error getting audio device ({}) info: {}", i, e.what());
            }
        }
    }

    void select(std::string name) {
        if (devices.empty()) {
            selectedDevice.clear();
            return;
        }

        // Check that such a device exist. If not select first
        if (!devices.keyExists(name)) {
            select(devices.key(0));
            return;
        }
        
        // Get device info
        devId = devices.keyId(name);
        auto info = devices.value(devId).info;
        selectedDevice = name;

        // List samplerates and save ID of the preference one
        sampleRates.clear();
        for (const auto& sr : info.sampleRates) {
            std::string name = getBandwdithScaled(sr);
            sampleRates.define(sr, name, sr);
            if (sr == info.preferredSampleRate) {
                srId = sampleRates.valueId(sr);
            }
        }

        // Load samplerate from config
        config.acquire();
        if (config.conf["devices"][selectedDevice].contains("sampleRate")) {
            sampleRate = config.conf["devices"][selectedDevice]["sampleRate"];
            if (sampleRates.keyExists(sampleRate)) {
                srId = sampleRates.keyId(sampleRate);
            }
        }
        config.release();

        // Update samplerate from ID
        sampleRate = sampleRates[srId];
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
        AudioSourceModule* _this = (AudioSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("AudioSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        AudioSourceModule* _this = (AudioSourceModule*)ctx;
        flog::info("AudioSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        AudioSourceModule* _this = (AudioSourceModule*)ctx;
        if (_this->running) { return; }
        
        // Stream options
        RtAudio::StreamParameters parameters;
        parameters.deviceId = _this->devices[_this->devId].id;
        parameters.nChannels = 2;
        unsigned int bufferFrames = _this->sampleRate / 200;
        RtAudio::StreamOptions opts;
        opts.flags = RTAUDIO_MINIMIZE_LATENCY;
        opts.streamName = "SDR++ Audio Source";

        // Open and start stream
        try {
            _this->audio.openStream(NULL, &parameters, RTAUDIO_FLOAT32, _this->sampleRate, &bufferFrames, callback, _this, &opts);
            _this->audio.startStream();
            _this->running = true;
        }
        catch (const std::exception& e) {
            flog::error("Error opening audio device: {}", e.what());
        }
        
        flog::info("AudioSourceModule '{}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        AudioSourceModule* _this = (AudioSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        _this->audio.stopStream();
        _this->audio.closeStream();

        flog::info("AudioSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        // Not possible
    }

    static void menuHandler(void* ctx) {
        AudioSourceModule* _this = (AudioSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_audio_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            std::string dev = _this->devices.key(_this->devId);
            _this->select(dev);
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["device"] = dev;
            config.release(true);
        }

        if (SmGui::Combo(CONCAT("##_audio_sr_sel_", _this->name), &_this->srId, _this->sampleRates.txt)) {
            _this->sampleRate = _this->sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            if (!_this->selectedDevice.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedDevice]["sampleRate"] = _this->sampleRate;
                config.release(true);
            }
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_audio_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedDevice);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }
    }

    static int callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData) {
        AudioSourceModule* _this = (AudioSourceModule*)userData;
        memcpy(_this->stream.writeBuf, inputBuffer, nBufferFrames * sizeof(dsp::complex_t));
        _this->stream.swap(nBufferFrames);
        return 0;
    }

#if RTAUDIO_VERSION_MAJOR >= 6
    static void errorCallback(RtAudioErrorType type, const std::string& errorText) {
        switch (type) {
        case RtAudioErrorType::RTAUDIO_NO_ERROR:
            return;
        case RtAudioErrorType::RTAUDIO_WARNING:
        case RtAudioErrorType::RTAUDIO_NO_DEVICES_FOUND:
        case RtAudioErrorType::RTAUDIO_DEVICE_DISCONNECT:
            flog::warn("AudioSourceModule Warning: {} ({})", errorText, (int)type);
            break;
        default:
            throw std::runtime_error(errorText);
        }
    }
#endif

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    
    OptionList<std::string, DeviceInfo> devices;
    OptionList<double, double> sampleRates;
    std::string selectedDevice = "";
    int devId = 0;
    int srId = 0;

    RtAudio audio;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/audio_source_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new AudioSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (AudioSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
