#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <dsp/buffer/packer.h>
#include <dsp/convert/stereo_to_mono.h>
#include <utils/flog.h>
#include <utils/optionlist.h>
#include <RtAudio.h>
#include <config.h>
#include <core.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "audio_sink",
    /* Description:     */ "Audio sink module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

bool operator==(const RtAudio::DeviceInfo& a, const RtAudio::DeviceInfo& b) {
    return a.name == b.name;
}

class AudioSink : public Sink {
public:
    AudioSink(SinkEntry* entry, dsp::stream<dsp::stereo_t>* stream, const std::string& name, SinkID id, const std::string& stringId) :
        Sink(entry, stream, name, id, stringId)
    {
        s2m.init(stream);
        monoPacker.init(&s2m.out, 512);
        stereoPacker.init(stream, 512);

#if RTAUDIO_VERSION_MAJOR >= 6
        audio.setErrorCallback(&errorCallback);
#endif

#if RTAUDIO_VERSION_MAJOR >= 6
        audio.setErrorCallback(&errorCallback);
#endif

        // Load config (TODO)
        bool created = false;
        std::string device = "";
        // config.acquire();
        // if (config.conf.contains(streamName)) {
        //     if (!config.conf[streamName].is_array()) {
        //         json tmp = config.conf[streamName];
        //         config.conf[streamName] = json::array();
        //         config.conf[streamName][0] = tmp;
        //         modified = true;
        //     }
        //     if (config.conf[streamName].contains((int)id)) {
        //         device = config.conf[streamName][(int)id]["device"];
        //     }
        // }
        // config.release(modified);

        // List devices
        RtAudio::DeviceInfo info;
#if RTAUDIO_VERSION_MAJOR >= 6
        for (int i : audio.getDeviceIds()) {
#else
        int count = audio.getDeviceCount();
        for (int i = 0; i < count; i++) {
#endif
            try {
                info = audio.getDeviceInfo(i);
#if !defined(RTAUDIO_VERSION_MAJOR) || RTAUDIO_VERSION_MAJOR < 6
                if (!info.probed) { continue; }
#endif
                if (info.outputChannels == 0) { continue; }
                if (info.isDefaultOutput) { defaultDevId = devList.size(); }
                devList.define(i, info.name, info);
            }
            catch (const std::exception& e) {
                flog::error("AudioSinkModule Error getting audio device ({}) info: {}", i, e.what());
            }
        }

        selectByName(device);
    }

    ~AudioSink() {
        stop();
    }

    void start() {
        if (running) { return; }
        running = doStart();
    }

    void stop() {
        if (!running) { return; }
        doStop();
        running = false;
    }

    void selectFirst() {
        selectById(defaultDevId);
    }

    void selectByName(std::string name) {
        for (int i = 0; i < devList.size(); i++) {
            if (devList[i].name == name) {
                selectById(i);
                return;
            }
        }
        selectFirst();
    }

    void selectById(int id) {
        // Update ID
        devId = id;
        selectedDevName = devList[id].name;

        // List samplerates and select default SR
        char buf[256];
        sampleRates.clear();
        const auto& srList = devList[id].sampleRates;
        unsigned int defaultSr = devList[id].preferredSampleRate;
        for (auto& sr : srList) {
            if (sr == defaultSr) {
                srId = sampleRates.size();
                sampleRate = sr;
            }
            sprintf(buf, "%d", sr);
            sampleRates.define(sr, buf, sr);
        }
    
        // // Load config
        // config.acquire();
        // if (config.conf[streamName][(int)id].contains(selectedDevName)) {
        //     unsigned int wantedSr = config.conf[streamName][id][selectedDevName];
        //     if (sampleRates.keyExists(wantedSr)) {
        //         srId = sampleRates.keyId(wantedSr);
        //         sampleRate = sampleRates[srId];
        //     }
        // }
        // config.release();

        // Lock the sink
        auto lck = entry->getLock();

        // Stop the sink DSP
        // TODO: Only if the sink DSP is running, otherwise you risk starting it when  it shouldn't
        entry->stopDSP();

        // Stop the sink
        if (running) { doStop(); }

        // Update stream samplerate
        entry->setSamplerate(sampleRate);

        // Start the DSP
        entry->startDSP();

        // Start the sink
        if (running) { doStart(); }
    }

    void showMenu() {
        float menuWidth = ImGui::GetContentRegionAvail().x;

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(("##_audio_sink_dev_" + stringId).c_str(), &devId, devList.txt)) {
            selectById(devId);
            // config.acquire();
            // config.conf[streamName]["device"] = devList[devId].name;
            // config.release(true);
        }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(("##_audio_sink_sr_" + stringId).c_str(), &srId, sampleRates.txt)) {
            sampleRate = sampleRates[srId];
            
            // Lock the sink
            auto lck = entry->getLock();

            // Stop the sink DSP
            // TODO: Only if the sink DSP is running, otherwise you risk starting it when  it shouldn't
            entry->stopDSP();

            // Stop the sink
            if (running) { doStop(); }

            // Update stream samplerate
            entry->setSamplerate(sampleRate);

            // Start the DSP
            entry->startDSP();

            // Start the sink
            if (running) { doStart(); }

            // config.acquire();
            // config.conf[streamName]["devices"][devList[devId].name] = sampleRate;
            // config.release(true);
        }
    }

#if RTAUDIO_VERSION_MAJOR >= 6
    static void errorCallback(RtAudioErrorType type, const std::string& errorText) {
        switch (type) {
        case RtAudioErrorType::RTAUDIO_NO_ERROR:
            return;
        case RtAudioErrorType::RTAUDIO_WARNING:
        case RtAudioErrorType::RTAUDIO_NO_DEVICES_FOUND:
        case RtAudioErrorType::RTAUDIO_DEVICE_DISCONNECT:
            flog::warn("AudioSinkModule Warning: {} ({})", errorText, (int)type);
            break;
        default:
            throw std::runtime_error(errorText);
        }
    }
#endif

private:
    bool doStart() {
        RtAudio::StreamParameters parameters;
        parameters.deviceId = devList.key(devId);
        parameters.nChannels = 2;
        unsigned int bufferFrames = sampleRate / 60;
        RtAudio::StreamOptions opts;
        opts.flags = RTAUDIO_MINIMIZE_LATENCY;
        opts.streamName = streamName;

        try {
            audio.openStream(&parameters, NULL, RTAUDIO_FLOAT32, sampleRate, &bufferFrames, &callback, this, &opts);
            stereoPacker.setSampleCount(bufferFrames);
            audio.startStream();
            stereoPacker.start();
        }
        catch (const std::exception& e) {
            flog::error("Could not open audio device {0}", e.what());
            return false;
        }

        flog::info("RtAudio stream open");
        return true;
    }

    void doStop() {
        s2m.stop();
        monoPacker.stop();
        stereoPacker.stop();
        monoPacker.out.stopReader();
        stereoPacker.out.stopReader();
        audio.stopStream();
        audio.closeStream();
        monoPacker.out.clearReadStop();
        stereoPacker.out.clearReadStop();
    }

    static int callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData) {
        AudioSink* _this = (AudioSink*)userData;
        int count = _this->stereoPacker.out.read();
        if (count < 0) { return 0; }

        memcpy(outputBuffer, _this->stereoPacker.out.readBuf, nBufferFrames * sizeof(dsp::stereo_t));
        _this->stereoPacker.out.flush();
        return 0;
    }

    dsp::convert::StereoToMono s2m;
    dsp::buffer::Packer<float> monoPacker;
    dsp::buffer::Packer<dsp::stereo_t> stereoPacker;

    int srId = 0;
    int devId = 0;
    bool running = false;
    std::string selectedDevName;

    unsigned int defaultDevId = 0;

    OptionList<unsigned int, RtAudio::DeviceInfo> devList;
    OptionList<unsigned int, unsigned int> sampleRates;

    unsigned int sampleRate = 48000;

    RtAudio audio;
};

class AudioSinkModule : public ModuleManager::Instance, public SinkProvider {
public:
    AudioSinkModule(std::string name) {
        this->name = name;
        sigpath::streamManager.registerSinkProvider("Audio", this);
    }

    ~AudioSinkModule() {
        sigpath::streamManager.unregisterSinkProvider(this);
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

    std::unique_ptr<Sink> createSink(SinkEntry* entry, dsp::stream<dsp::stereo_t>* stream, const std::string& name, SinkID id, const std::string& stringId) {
        return std::make_unique<AudioSink>(entry, stream, name, id, stringId);
    }

private:
    std::string name;
    bool enabled = true;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(core::args["root"].s() + "/audio_sink_config.json");
    config.load(def);
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