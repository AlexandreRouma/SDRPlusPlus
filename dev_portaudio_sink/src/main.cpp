#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <config.h>
#include <options.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <dsp/audio.h>
#include <dsp/processing.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <utility>
#include "portaudio_interface.h"

SDRPP_MOD_INFO {
        /* Name:            */ "portaudio_sink",
        /* Description:     */ "Audio sink module for SDR++",
        /* Author:          */ "Maxime Biette",
        /* Version:         */ 0, 1, 0,
        /* Max instances    */ 1
};


ConfigManager config;

class PortaudioSink : SinkManager::Sink {
public:
    PortaudioSink(SinkManager::Stream *stream, std::string streamName):
        _stream{stream},
        _streamName{std::move(streamName)}
    {
        s2m.init(_stream->sinkOut);
        monoPacker.init(&s2m.out, 512);
        stereoPacker.init(_stream->sinkOut, 512);

        bool created = false;
        std::string device;
        config.acquire();
        if (!config.conf.contains(_streamName)) {
            created = true;
            config.conf[_streamName]["device"] = "";
            config.conf[_streamName]["devices"] = json({});
        }
        device = config.conf[_streamName]["device"];
        config.release(created);

        deviceList = PortaudioInterface::getDeviceList();
    }

    ~PortaudioSink() override = default;

    void start() override {
        if (running) {
            return;
        }
        doStart();
        running = true;
    }

    void stop() override {
        if (!running) {
            return;
        }
        doStop();
        running = false;
    }

    void selectFirst() {
        selectById(0);
    }

    void selectByName(const std::string &name) {
        const auto device = std::find_if(
                deviceList.cbegin(),
                deviceList.cend(),
                [&name](const auto &device) { return device.name == name; }
        );
        if (device != deviceList.cend()) selectDevice(*device);
        else selectFirst();
    }

    void selectById(int id) {
        if (deviceList.size() <= id) return;
        const auto &device = deviceList[id];
        selectDevice(device);
    }

    void selectDevice(const AudioDevice_t &device) {
        bool created = false;
        config.acquire();
        if (!config.conf[_streamName]["devices"].contains(device.name)) {
            created = true;
            config.conf[_streamName]["devices"][device.name] = 48000;
        }
        auto sampleRate = config.conf[_streamName]["devices"][device.name];
        config.release(created);

        const auto &sampleRates = device.sampleRates;
        sampleRate = sampleRates[device.sampleRateId];
        _stream->setSampleRate((float)sampleRate);

        if (running) { doStop(); }
        if (running) { doStart(); }
    }



    void menuHandler() override {
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(("##_audio_sink_dev_"+_streamName).c_str(), &devId,
                         formatDeviceListText().c_str())) {
            if (deviceList.size() > devId) {
                selectById(devId);
                config.acquire();
                config.conf[_streamName]["device"] = deviceList[devId].name;
                config.release(true);
            }
        }

        if (deviceList.size() <= devId) {
            if (running) doStop();
            return;
        }
        auto &device = deviceList[devId];

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(("##_audio_sink_sr_"+_streamName).c_str(), &(device.sampleRateId),
                         formatSampleRatesListText(device).c_str())) {
            auto sampleRate = (unsigned int)device.sampleRates[device.sampleRateId];
            _stream->setSampleRate((float)sampleRate);
            if (running) {
                doStop();
                doStart();
            }
            config.acquire();
            config.conf[_streamName]["devices"][device.name] = sampleRate;
            config.release(true);
        }
    }
private:
    void doStart() {
        if (deviceList.size() <= devId) return;
        const auto &device = deviceList[devId];
        float sampleRate = device.sampleRates[device.sampleRateId];
        int bufferFrames = (int)sampleRate / 60;
        if (device.channels == 2) {
            stereoPacker.setSampleCount(bufferFrames);
            if (!audioInterface.open<dsp::stream<dsp::stereo_t>, dsp::stereo_t>(device, &stereoPacker.out, sampleRate)){
                spdlog::info("Audio device error.");
                running = false;
                return;
            }
            stereoPacker.start();
        }
        else {
            monoPacker.setSampleCount(bufferFrames);
            if (!audioInterface.open<dsp::stream<dsp::mono_t>, dsp::mono_t>(device, &monoPacker.out, sampleRate)) {
                spdlog::info("Audio device error.");
                running = false;
                return;
            }
            monoPacker.start();
        }
        spdlog::info("Audio device open.");
        running = true;
    }
    void doStop() {
        s2m.stop();
        monoPacker.stop();
        stereoPacker.stop();
        monoPacker.out.stopReader();
        stereoPacker.out.stopReader();
        audioInterface.close();
        monoPacker.out.clearReadStop();
        stereoPacker.out.clearReadStop();
    }

    std::string formatDeviceListText() {
        std::stringstream textList;
        for(const auto &device: deviceList){
            textList << device.name;
            textList << '\0';
        }
        return textList.str();
    }

    static std::string formatSampleRatesListText(const AudioDevice_t &device) {
        std::stringstream textList;
        for(const auto sr: device.sampleRates){
            textList << sr;
            textList << '\0';
        }
        return textList.str();
    }

    SinkManager::Stream* _stream;
    dsp::StereoToMono s2m;
    dsp::Packer<dsp::mono_t> monoPacker;
    dsp::Packer<dsp::stereo_t> stereoPacker;

    std::string _streamName;

    bool running = false;
    int devId = 0;
    std::vector<AudioDevice_t> deviceList;
    PortaudioInterface audioInterface{};
};

class PortaudioSinkModule : public ModuleManager::Instance {
public:
    explicit PortaudioSinkModule(std::string name) :
        provider{create_sink, this}
    {
        sigpath::sinkManager.registerSinkProvider("Portaudio", provider);
    }

    ~PortaudioSinkModule() = default;

    void enable() override {
        enabled = true;
    }

    void disable() override {
        enabled = false;
    }

    bool isEnabled() override {
        return enabled;
    }

private:
    static SinkManager::Sink* create_sink(SinkManager::Stream* stream, std::string streamName, void* ctx) {
        return (SinkManager::Sink*)(new PortaudioSink(stream, std::move(streamName)));
    }

    bool enabled = true;
    SinkManager::SinkProvider provider;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(options::opts.root + "/dev_portaudio_sink_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    auto instance = new PortaudioSinkModule(std::move(name));
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete static_cast<PortaudioSinkModule *>(instance);
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}