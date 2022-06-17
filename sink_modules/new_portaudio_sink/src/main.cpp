#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <portaudio.h>
#include <dsp/buffer/packer.h>
#include <dsp/convert/stereo_to_mono.h>
#include <spdlog/spdlog.h>
#include <config.h>
#include <algorithm>
#include <core.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

#define BLOCK_SIZE_DIVIDER 60
#define AUDIO_LATENCY      1.0 / 60.0

SDRPP_MOD_INFO{
    /* Name:            */ "new_portaudio_sink",
    /* Description:     */ "Audio sink module for SDR++",
    /* Author:          */ "Ryzerth;Maxime Biette",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class AudioSink : SinkManager::Sink {
public:
    struct AudioDevice_t {
        const PaDeviceInfo* deviceInfo;
        const PaHostApiInfo* hostApiInfo;
        PaDeviceIndex id;
        int defaultSrId;
        PaStreamParameters outputParams;
        std::vector<double> sampleRates;
        std::string sampleRatesTxt;
    };

    AudioSink(SinkManager::Stream* stream, std::string streamName) {
        _stream = stream;
        _streamName = streamName;

        // Create config if it doesn't exist
        config.acquire();
        if (!config.conf.contains(_streamName)) {
            config.conf[_streamName]["device"] = "";
            config.conf[_streamName]["devices"] = json::object();
        }
        std::string selected = config.conf[_streamName]["device"];
        config.release(true);

        // Register the play state handler
        playStateHandler.handler = playStateChangeHandler;
        playStateHandler.ctx = this;
        gui::mainWindow.onPlayStateChange.bindHandler(&playStateHandler);

        // Initialize DSP blocks
        packer.init(_stream->sinkOut, 1024);
        s2m.init(&packer.out);

        // Refresh devices and select the one from the config
        refreshDevices();
        selectDevByName(selected);
    }

    ~AudioSink() {
        stop();
        gui::mainWindow.onPlayStateChange.unbindHandler(&playStateHandler);
    }

    void start() {
        if (running || selectedDevName.empty()) { return; }

        // Get device and samplerate
        AudioDevice_t& dev = devices[deviceNames[devId]];
        double sampleRate = dev.sampleRates[srId];
        int blockSize = sampleRate / BLOCK_SIZE_DIVIDER;

        // Set the SDR++ stream sample rate
        _stream->setSampleRate(sampleRate);

        // Update the block size on the packer
        packer.setSampleCount(blockSize);

        // Clear read stop signals
        packer.out.clearReadStop();
        s2m.out.clearReadStop();

        // Open the stream
        PaError err;
        if (dev.deviceInfo->maxOutputChannels == 1) {
            packer.start();
            s2m.start();
            stereo = false;
            err = Pa_OpenStream(&devStream, NULL, &dev.outputParams, sampleRate, blockSize, paNoFlag, _mono_cb, this);
        }
        else {
            packer.start();
            stereo = true;
            err = Pa_OpenStream(&devStream, NULL, &dev.outputParams, sampleRate, blockSize, paNoFlag, _stereo_cb, this);
        }

        // In case of error, abort
        if (err) {
            spdlog::error("PortAudio error {0}: {1}", err, Pa_GetErrorText(err));
            return;
        }

        spdlog::info("Starting PortAudio stream at {0} S/s", sampleRate);

        // Start stream
        Pa_StartStream(devStream);

        running = true;
    }

    void stop() {
        if (!running || selectedDevName.empty()) { return; }

        // Send stop signal to the streams
        packer.out.stopReader();
        s2m.out.stopReader();

        // Stop DSP
        packer.stop();
        s2m.stop();

        // Stop stream
        Pa_AbortStream(devStream);

        // Close the stream
        Pa_CloseStream(devStream);

        running = false;
    }

    void menuHandler() {
        float menuWidth = ImGui::GetContentRegionAvail().x;

        // Select device
        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo("##audio_sink_dev_sel", &devId, deviceNamesTxt.c_str())) {
            selectDevByName(deviceNames[devId]);
            stop();
            start();
            if (selectedDevName != "") {
                config.acquire();
                config.conf[_streamName]["device"] = selectedDevName;
                config.release(true);
            }
        }

        // Select sample rate
        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo("##audio_sink_sr_sel", &srId, selectedDev.sampleRatesTxt.c_str())) {
            stop();
            start();
            if (selectedDevName != "") {
                config.acquire();
                config.conf[_streamName]["devices"][selectedDevName] = selectedDev.sampleRates[srId];
                config.release(true);
            }
        }
    }

    int devId = 0;
    int srId = 0;
    bool stereo = false;

private:
    static void playStateChangeHandler(bool newState, void* ctx) {
        AudioSink* _this = (AudioSink*)ctx;

        // Wake up reader to send nulls instead of data in preparation for shutoff
        if (newState) {
            if (_this->stereo) {
                _this->packer.out.stopReader();
            }
            else {
                _this->s2m.out.stopReader();
            }
        }
        else {
            if (_this->stereo) {
                _this->packer.out.clearReadStop();
            }
            else {
                _this->s2m.out.clearReadStop();
            }
        }
    }

    void refreshDevices() {
        // Clear current list
        devices.clear();
        deviceNames.clear();
        deviceNamesTxt.clear();

        // Get number of devices
        int devCount = Pa_GetDeviceCount();
        PaStreamParameters outputParams;
        char buffer[128];

        for (int i = 0; i < devCount; i++) {
            AudioDevice_t dev;

            // Get device info
            dev.deviceInfo = Pa_GetDeviceInfo(i);
            dev.hostApiInfo = Pa_GetHostApiInfo(dev.deviceInfo->hostApi);
            dev.id = i;

            // Check if device is usable
            if (dev.deviceInfo->maxOutputChannels == 0) { continue; }
#ifdef _WIN32
            // On Windows, use only WASAPI
            if (dev.hostApiInfo->type == paMME || dev.hostApiInfo->type == paWDMKS) { continue; }
#endif
            // Zero out output params
            dev.outputParams.device = i;
            dev.outputParams.sampleFormat = paFloat32;
            dev.outputParams.suggestedLatency = std::min<PaTime>(AUDIO_LATENCY, dev.deviceInfo->defaultLowOutputLatency);
            dev.outputParams.channelCount = std::min<int>(dev.deviceInfo->maxOutputChannels, 2);
            dev.outputParams.hostApiSpecificStreamInfo = NULL;

            // List available sample rates
            for (int sr = 12000; sr < 200000; sr += 12000) {
                if (Pa_IsFormatSupported(NULL, &dev.outputParams, sr) != paFormatIsSupported) { continue; }
                dev.sampleRates.push_back(sr);
            }
            for (int sr = 11025; sr < 192000; sr += 11025) {
                if (Pa_IsFormatSupported(NULL, &dev.outputParams, sr) != paFormatIsSupported) { continue; }
                dev.sampleRates.push_back(sr);
            }

            // If no sample rates are supported, cancel adding device
            if (dev.sampleRates.empty()) {
                continue;
            }

            // Sort sample rate list
            std::sort(dev.sampleRates.begin(), dev.sampleRates.end(), [](double a, double b) { return (a < b); });

            // Generate text list for UI
            int srId = 0;
            int _48kId = -1;
            for (auto sr : dev.sampleRates) {
                sprintf(buffer, "%d", (int)sr);
                dev.sampleRatesTxt += buffer;
                dev.sampleRatesTxt += '\0';

                // Save ID of the default sample rate and 48KHz
                if (sr == dev.deviceInfo->defaultSampleRate) { dev.defaultSrId = srId; }
                if (sr == 48000.0) { _48kId = srId; }
                srId++;
            }

            // If a 48KHz option was found, use it instead of the default
            if (_48kId >= 0) { dev.defaultSrId = _48kId; }

            std::string apiName = dev.hostApiInfo->name;

#ifdef _WIN32
            // Shorten the names on windows
            if (apiName.rfind("Windows ", 0) == 0) {
                apiName = apiName.substr(8);
            }
#endif
            // Create device name and save to list
            sprintf(buffer, "[%s] %s", apiName.c_str(), dev.deviceInfo->name);
            devices[buffer] = dev;
            deviceNames.push_back(buffer);
            deviceNamesTxt += buffer;
            deviceNamesTxt += '\0';
        }
    }

    void selectDefault() {
        if (devices.empty()) {
            selectedDevName = "";
            return;
        }

        // Search for the default device
        PaDeviceIndex defId = Pa_GetDefaultOutputDevice();
        for (auto const& [name, dev] : devices) {
            if (dev.id != defId) { continue; }
            selectDevByName(name);
            return;
        }

        // If default not found, select first
        selectDevByName(deviceNames[0]);
    }

    void selectDevByName(std::string name) {
        auto devIt = std::find(deviceNames.begin(), deviceNames.end(), name);
        if (devIt == deviceNames.end()) {
            selectDefault();
            return;
        }

        // Load the device name, device descriptor and device ID
        selectedDevName = name;
        selectedDev = devices[name];
        devId = std::distance(deviceNames.begin(), devIt);

        // Load config
        config.acquire();
        if (!config.conf[_streamName]["devices"].contains(name)) {
            config.conf[_streamName]["devices"][name] = selectedDev.sampleRates[selectedDev.defaultSrId];
        }
        config.release(true);

        // Find the sample rate ID, if not use default
        bool found = false;
        double selectedSr = config.conf[_streamName]["devices"][name];
        for (int i = 0; i < selectedDev.sampleRates.size(); i++) {
            if (selectedDev.sampleRates[i] != selectedSr) { continue; }
            srId = i;
            found = true;
            break;
        }
        if (!found) {
            srId = selectedDev.defaultSrId;
        }
    }

    static int _mono_cb(const void* input, void* output, unsigned long frameCount,
                        const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData) {
        AudioSink* _this = (AudioSink*)userData;

        // For OSX, mute audio when not playing
        if (!gui::mainWindow.isPlaying()) {
            memset(output, 0, frameCount * sizeof(float));
            _this->s2m.out.flush();
            return 0;
        }

        // Write to buffer
        _this->s2m.out.read();
        memcpy(output, _this->s2m.out.readBuf, frameCount * sizeof(float));
        _this->s2m.out.flush();
        return 0;
    }

    static int _stereo_cb(const void* input, void* output, unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData) {
        AudioSink* _this = (AudioSink*)userData;

        // For OSX, mute audio when not playing
        if (!gui::mainWindow.isPlaying()) {
            memset(output, 0, frameCount * sizeof(dsp::stereo_t));
            _this->packer.out.flush();
            return 0;
        }

        // Write to buffer
        _this->packer.out.read();
        memcpy(output, _this->packer.out.readBuf, frameCount * sizeof(dsp::stereo_t));
        _this->packer.out.flush();
        return 0;
    }

    std::string _streamName;

    bool running = false;
    std::map<std::string, AudioDevice_t> devices;
    std::vector<std::string> deviceNames;
    std::string deviceNamesTxt;

    AudioDevice_t selectedDev;
    std::string selectedDevName;

    SinkManager::Stream* _stream;
    dsp::buffer::Packer<dsp::stereo_t> packer;
    dsp::convert::StereoToMono s2m;

    PaStream* devStream;

    EventHandler<bool> playStateHandler;
};

class AudioSinkModule : public ModuleManager::Instance {
public:
    AudioSinkModule(std::string name) {
        this->name = name;
        provider.create = create_sink;
        provider.ctx = this;

        Pa_Initialize();

        sigpath::sinkManager.registerSinkProvider("New Audio", provider);
    }

    ~AudioSinkModule() {
        sigpath::sinkManager.unregisterSinkProvider("New Audio");
        Pa_Terminate();
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

private:
    static SinkManager::Sink* create_sink(SinkManager::Stream* stream, std::string streamName, void* ctx) {
        return (SinkManager::Sink*)(new AudioSink(stream, streamName));
    }

    std::string name;
    bool enabled = true;
    SinkManager::SinkProvider provider;
};

MOD_EXPORT void _INIT_() {
    config.setPath(core::args["root"].s() + "/new_audio_sink_config.json");
    config.load(json::object());
    config.enableAutoSave();
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    AudioSinkModule* instance = new AudioSinkModule(name);
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (AudioSinkModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}