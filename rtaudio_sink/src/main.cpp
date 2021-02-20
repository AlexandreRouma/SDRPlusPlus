#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <dsp/audio.h>
#include <dsp/processing.h>
#include <spdlog/spdlog.h>
#include <RtAudio.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "rtaudio_sink",
    /* Description:     */ "RtAudio sink module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

class AudioSink : SinkManager::Sink {
public:
    AudioSink(SinkManager::Stream* stream, std::string streamName) {
        _stream = stream;
        _streamName = streamName;
        s2m.init(_stream->sinkOut);
        monoPacker.init(&s2m.out, 512);
        stereoPacker.init(_stream->sinkOut, 512);

        stream->setSampleRate(48000);

        RtAudio::StreamParameters parameters;

        int count = audio.getDeviceCount();
        RtAudio::DeviceInfo info;
        for (int i = 0; i < count; i++) {
            info = audio.getDeviceInfo(i);
            if (!info.probed) { continue; }
            if (info.outputChannels == 0) { continue; }
            if (info.isDefaultOutput) { devId = devList.size(); }
            devList.push_back(info);
            deviceIds.push_back(i);
        }
    }

    ~AudioSink() {
        
    }

    void start() {
        if (running) {
            return;
        }
        doStart();
        running = true;
    }

    void stop() {
        if (!running) {
            return;
        }
        doStop();
        running = false;
    }
    
    void menuHandler() {
        float menuWidth = ImGui::GetContentRegionAvailWidth();

    //     ImGui::SetNextItemWidth(menuWidth);
    //     if (ImGui::Combo(("##_rtaudio_sink_dev_"+_streamName).c_str(), &devListId, txtDevList.c_str())) {
    //         // TODO: Load SR from config
    //         if (running) {
    //             doStop();
    //             doStart();
    //         }
    //         // TODO: Save to config
    //     }

    //     AudioDevice_t* dev = devices[devListId];

    //     ImGui::SetNextItemWidth(menuWidth);
    //     if (ImGui::Combo(("##_rtaudio_sink_sr_"+_streamName).c_str(), &dev->srId, dev->txtSampleRates.c_str())) {
    //         _stream->setSampleRate(dev->sampleRates[dev->srId]);
    //         if (running) {
    //             doStop();
    //             doStart();
    //         }
    //         // TODO: Save to config
    //     }
    }

private:
    void doStart() {
            RtAudio::StreamParameters parameters;
            parameters.deviceId = deviceIds[devId];
            parameters.nChannels = 2;
            unsigned int sampleRate = 48000;
            unsigned int bufferFrames = sampleRate / 200;
            RtAudio::StreamOptions opts;
            opts.flags = RTAUDIO_MINIMIZE_LATENCY;

            stereoPacker.setSampleCount(bufferFrames);

            try {
                audio.openStream(&parameters, NULL, RTAUDIO_FLOAT32, sampleRate, &bufferFrames, &callback, this, &opts);
                audio.startStream();
                stereoPacker.start();
            }
            catch ( RtAudioError& e ) {
                spdlog::error("Could not open audio device");
            }
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
        stereoPacker.out.clearWriteStop();
    }

    static int callback( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void *userData) {
        AudioSink* _this = (AudioSink*)userData;
        int count = _this->stereoPacker.out.read();
        if (count < 0) { return 0; }
        memcpy(outputBuffer, _this->stereoPacker.out.readBuf, nBufferFrames * sizeof(dsp::stereo_t));
        _this->stereoPacker.out.flush();
        return 0;
    }
    
    SinkManager::Stream* _stream;
    dsp::StereoToMono s2m;
    dsp::Packer<float> monoPacker;
    dsp::Packer<dsp::stereo_t> stereoPacker;

    std::string _streamName;

    int srId = 0;
    int devCount;
    int devId = 0;
    bool running = false;

    const double POSSIBLE_SAMP_RATE[6] = {
        48000.0f,
        44100.0f,
        24000.0f,
        22050.0f,
        12000.0f,
        11025.0f
    };


    std::vector<RtAudio::DeviceInfo> devList;
    std::vector<unsigned int> deviceIds;
    std::string txtDevList;

    RtAudio audio;

};

class AudioSinkModule : public ModuleManager::Instance {
public:
    AudioSinkModule(std::string name) {
        this->name = name;
        provider.create = create_sink;
        provider.ctx = this;

        sigpath::sinkManager.registerSinkProvider("RtAudio", provider);
    }

    ~AudioSinkModule() {
        
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

private:
    static SinkManager::Sink* create_sink(SinkManager::Stream* stream, std::string streamName, void* ctx) {
        return (SinkManager::Sink*)(new AudioSink(stream, streamName));
    }

    std::string name;
    bool enabled = true;
    SinkManager::SinkProvider provider;

};

MOD_EXPORT void _INIT_() {
    // Nothing here
    // TODO: Do instancing here (in source modules as well) to prevent multiple loads
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    AudioSinkModule* instance = new AudioSinkModule(name);
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_() {
    
}

MOD_EXPORT void _END_() {
    
}