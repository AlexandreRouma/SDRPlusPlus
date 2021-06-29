#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <portaudio.h>
#include <dsp/audio.h>
#include <dsp/processing.h>
#include <spdlog/spdlog.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "audio_sink",
    /* Description:     */ "Audio sink module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

class AudioSink : SinkManager::Sink {
public:
    struct AudioDevice_t {
        std::string name;
        int index;
        int channels;
        int srId;
        std::vector<double> sampleRates;
        std::string txtSampleRates;
    };
    
    AudioSink(SinkManager::Stream* stream, std::string streamName) {
        _stream = stream;
        _streamName = streamName;
        s2m.init(_stream->sinkOut);
        monoRB.init(&s2m.out);
        stereoRB.init(_stream->sinkOut);

        // monoPacker.init(&s2m.out, 240);
        // stereoPacker.init(_stream->sinkOut, 240);

        // Initialize PortAudio
        devCount = Pa_GetDeviceCount();
        devId = Pa_GetDefaultOutputDevice();
        const PaDeviceInfo *deviceInfo;
        PaStreamParameters outputParams;
        outputParams.sampleFormat = paFloat32;
        outputParams.hostApiSpecificStreamInfo = NULL;

        // Gather hardware info
        for(int i = 0; i < devCount; i++) {
            deviceInfo = Pa_GetDeviceInfo(i);
            if (deviceInfo->maxOutputChannels < 1) {
                continue;
            }
            AudioDevice_t dev;
            dev.name = deviceInfo->name;
            dev.index = i;
            dev.channels = std::min<int>(deviceInfo->maxOutputChannels, 2);
            dev.sampleRates.clear();
            dev.txtSampleRates = "";
            for (int j = 0; j < 6; j++) {
                outputParams.channelCount = dev.channels;
                outputParams.device = dev.index;
                outputParams.suggestedLatency = deviceInfo->defaultLowOutputLatency;
                PaError err = Pa_IsFormatSupported(NULL, &outputParams, POSSIBLE_SAMP_RATE[j]);
                if (err != paFormatIsSupported) {
                    continue;
                }
                dev.sampleRates.push_back(POSSIBLE_SAMP_RATE[j]);
                dev.txtSampleRates += std::to_string((int)POSSIBLE_SAMP_RATE[j]);
                dev.txtSampleRates += '\0';
            }
            if (dev.sampleRates.size() == 0) {
                continue;
            }
            if (i == devId) {
                devListId = devices.size();
                defaultDev = devListId;
                _stream->setSampleRate(dev.sampleRates[0]);
            }
            dev.srId = 0;

            AudioDevice_t* _dev = new AudioDevice_t;
            *_dev = dev;
            devices.push_back(_dev);

            deviceNames.push_back(deviceInfo->name);
            txtDevList += deviceInfo->name;
            txtDevList += '\0';
        }

        // Load config from file
    }

    ~AudioSink() {
        for (auto const& dev : devices) {
            delete dev;
        }
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

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(("##_audio_sink_dev_"+_streamName).c_str(), &devListId, txtDevList.c_str())) {
            // TODO: Load SR from config
            if (running) {
                doStop();
                doStart();
            }
            // TODO: Save to config
        }

        AudioDevice_t* dev = devices[devListId];

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(("##_audio_sink_sr_"+_streamName).c_str(), &dev->srId, dev->txtSampleRates.c_str())) {
            _stream->setSampleRate(dev->sampleRates[dev->srId]);
            if (running) {
                doStop();
                doStart();
            }
            // TODO: Save to config
        }
    }

private:
    void doStart() {
        const PaDeviceInfo *deviceInfo;
        AudioDevice_t* dev = devices[devListId];
        PaStreamParameters outputParams;
        deviceInfo = Pa_GetDeviceInfo(dev->index);
        outputParams.channelCount = 2;
        outputParams.sampleFormat = paFloat32;
        outputParams.hostApiSpecificStreamInfo = NULL;
        outputParams.device = dev->index;
        outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
        PaError err;

        float sampleRate = dev->sampleRates[dev->srId];
        int bufferSize = sampleRate / 60.0f;

        if (dev->channels == 2) {
            stereoRB.data.setMaxLatency(bufferSize * 2);
            stereoRB.start();
            // stereoPacker.setSampleCount(bufferSize);
            // stereoPacker.start();
            err = Pa_OpenStream(&stream, NULL, &outputParams, sampleRate, paFramesPerBufferUnspecified, 0, _stereo_cb, this);
            //err = Pa_OpenStream(&stream, NULL, &outputParams, sampleRate, bufferSize, 0, _stereo_cb, this);
        }
        else {
            monoRB.data.setMaxLatency(bufferSize * 2);
            monoRB.start();
            // stereoPacker.setSampleCount(bufferSize);
            // monoPacker.start();
            err = Pa_OpenStream(&stream, NULL, &outputParams, sampleRate, paFramesPerBufferUnspecified, 0, _mono_cb, this);
            //err = Pa_OpenStream(&stream, NULL, &outputParams, sampleRate, bufferSize, 0, _mono_cb, this);
        }

        if (err != 0) {
            spdlog::error("Error while opening audio stream: ({0}) => {1}", err, Pa_GetErrorText(err));
            return;
        }

        err = Pa_StartStream(stream);
        if (err != 0) {
            spdlog::error("Error while starting audio stream: ({0}) => {1}", err, Pa_GetErrorText(err));
            return;
        }
        spdlog::info("Audio device open.");
        running = true;
    }

    void doStop() {
        s2m.stop();
        monoRB.stop();
        stereoRB.stop();
        // monoPacker.stop();
        // stereoPacker.stop();
        monoRB.data.stopReader();
        stereoRB.data.stopReader();
        // monoPacker.out.stopReader();
        // stereoPacker.out.stopReader();
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        monoRB.data.clearReadStop();
        stereoRB.data.clearReadStop();
        // monoPacker.out.clearReadStop();
        // stereoPacker.out.clearWriteStop();
    }

    static int _mono_cb(const void *input, void *output, unsigned long frameCount,
        const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
        AudioSink* _this = (AudioSink*)userData;
        if (!gui::mainWindow.isPlaying()) { 
            memset(output, 0, frameCount*sizeof(float));
            return 0;
        }
        _this->monoRB.data.read((float*)output, frameCount);
        return 0;
    }

    static int _stereo_cb(const void *input, void *output, unsigned long frameCount,
        const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
        AudioSink* _this = (AudioSink*)userData;
        if (!gui::mainWindow.isPlaying()) { 
            memset(output, 0, frameCount*sizeof(dsp::stereo_t));
            return 0;
        }
        _this->stereoRB.data.read((dsp::stereo_t*)output, frameCount);
        return 0;
    }

    // static int _mono_cb(const void *input, void *output, unsigned long frameCount,
    //     const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
    //     AudioSink* _this = (AudioSink*)userData;
    //     if (_this->monoPacker.out.read() < 0) { return 0; }
    //     memcpy((float*)output, _this->monoPacker.out.readBuf, frameCount * sizeof(float));
    //     _this->monoPacker.out.flush();
    //     return 0;
    // }

    // static int _stereo_cb(const void *input, void *output, unsigned long frameCount,
    //     const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
    //     AudioSink* _this = (AudioSink*)userData;
    //     if (_this->stereoPacker.out.read() < 0) { spdlog::warn("CB killed"); return 0; }
    //     memcpy((dsp::stereo_t*)output, _this->stereoPacker.out.readBuf, frameCount * sizeof(dsp::stereo_t));
    //     _this->stereoPacker.out.flush();
    //     return 0;
    // }
    
    
    SinkManager::Stream* _stream;
    dsp::StereoToMono s2m;
    dsp::RingBufferSink<float> monoRB;
    dsp::RingBufferSink<dsp::stereo_t> stereoRB;

    // dsp::Packer<float> monoPacker;
    // dsp::Packer<dsp::stereo_t> stereoPacker;

    std::string _streamName;
    PaStream *stream;

    int srId = 0;
    int devCount;
    int devId = 0;
    int devListId = 0;
    int defaultDev = 0;
    bool running = false;

    const double POSSIBLE_SAMP_RATE[6] = {
        48000.0f,
        44100.0f,
        24000.0f,
        22050.0f,
        12000.0f,
        11025.0f
    };

    std::vector<AudioDevice_t*> devices;
    std::vector<std::string> deviceNames;
    std::string txtDevList;

};

class AudioSinkModule : public ModuleManager::Instance {
public:
    AudioSinkModule(std::string name) {
        this->name = name;
        provider.create = create_sink;
        provider.ctx = this;

        Pa_Initialize();

        sigpath::sinkManager.registerSinkProvider("Audio", provider);
    }

    ~AudioSinkModule() {
        Pa_Terminate();
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