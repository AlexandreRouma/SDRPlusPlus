#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <portaudio.h>
#include <dsp/audio.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

class AudioSink : SinkManager::Sink {
public:
    struct AudioDevice_t {
        std::string name;
        int index;
        int channels;
        std::vector<double> sampleRates;
        std::string txtSampleRates;
    };
    
    AudioSink(SinkManager::Stream* stream) {
        _stream = stream;
        audioStream = _stream->bindStream();
        s2m.init(audioStream);
        monoRB.init(&s2m.out);
        stereoRB.init(audioStream);

        // Initialize PortAudio
        Pa_Initialize();
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
            }
            devices.push_back(dev);
            deviceNames.push_back(deviceInfo->name);
            txtDevList += deviceInfo->name;
            txtDevList += '\0';
        }
    }

    ~AudioSink() {
        
    }

    void start() {
        
    }

    void stop() {

    }
    
    void menuHandler() {

    }

private:
    
    SinkManager::Stream* _stream;
    dsp::stream<dsp::stereo_t>* audioStream;
    dsp::StereoToMono s2m;
    dsp::RingBufferSink<float> monoRB;
    dsp::RingBufferSink<dsp::stereo_t> stereoRB;

    int srId = 0;
    float sampleRate;
    int devCount;
    int devId = 0;
    int devListId = 0;
    int defaultDev = 0;

    const double POSSIBLE_SAMP_RATE[6] = {
        48000.0f,
        44100.0f,
        24000.0f,
        22050.0f,
        12000.0f,
        11025.0f
    };

    std::vector<AudioDevice_t> devices;
    std::vector<std::string> deviceNames;
    std::string txtDevList;

};

class AudioSinkModule {
public:
    AudioSinkModule(std::string name) {
        this->name = name;
        provider.create = create_sink;
        provider.ctx = this;
        sigpath::sinkManager.registerSinkProvider("Audio", provider);
    }

    ~AudioSinkModule() {

    }

private:
    static SinkManager::Sink* create_sink(SinkManager::Stream* stream, void* ctx) {
        return (SinkManager::Sink*)(new AudioSink(stream));
    }

    std::string name;
    SinkManager::SinkProvider provider;

};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    AudioSinkModule* instance = new AudioSinkModule(name);
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_() {
    
}

MOD_EXPORT void _STOP_() {
    
}