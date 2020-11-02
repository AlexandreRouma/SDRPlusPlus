#pragma once
#include <dsp/stream.h>
#include <dsp/routing.h>
#include <dsp/audio.h>
#include <io/audio.h>
#include <map>

namespace audio {
    enum {
        STREAM_TYPE_MONO,
        STREAM_TYPE_STEREO,
        _STREAM_TYPE_COUNT
    };

    // TODO: Create a manager class and an instance class

    struct BoundStream_t {
        dsp::stream<float>* monoStream;
        dsp::stream<dsp::stereo_t>* stereoStream;
        dsp::StereoToMono* s2m;
        dsp::MonoToStereo* m2s;
        void (*streamRemovedHandler)(void* ctx);
        void (*sampleRateChangeHandler)(void* ctx, double sampleRate, int blockSize);
        void* ctx;
        int type;
    };

    struct AudioStream_t {
        io::AudioSink* audio;
        dsp::stream<float>* monoAudioStream;
        dsp::stream<dsp::stereo_t>* stereoAudioStream;
        std::vector<BoundStream_t> boundStreams;
        dsp::stream<float>* monoStream;
        dsp::Splitter<float>* monoDynSplit;
        dsp::stream<dsp::stereo_t>* stereoStream;
        dsp::Splitter<dsp::stereo_t>* stereoDynSplit;
        int (*sampleRateChangeHandler)(void* ctx, double sampleRate);
        double sampleRate;
        int blockSize;
        int type;
        bool running = false;
        float volume;
        int sampleRateId;
        int deviceId;
        void* ctx;
        std::string vfoName;
    };

    extern std::map<std::string, AudioStream_t*> streams;

    double registerMonoStream(dsp::stream<float>* stream, std::string name, std::string vfoName, int (*sampleRateChangeHandler)(void* ctx, double sampleRate), void* ctx);
    double registerStereoStream(dsp::stream<dsp::stereo_t>* stream, std::string name, std::string vfoName, int (*sampleRateChangeHandler)(void* ctx, double sampleRate), void* ctx);
    void startStream(std::string name);
    void stopStream(std::string name);
    void removeStream(std::string name);
    dsp::stream<float>* bindToStreamMono(std::string name, void (*streamRemovedHandler)(void* ctx), void (*sampleRateChangeHandler)(void* ctx, double sampleRate, int blockSize), void* ctx);
    dsp::stream<dsp::stereo_t>* bindToStreamStereo(std::string name, void (*streamRemovedHandler)(void* ctx), void (*sampleRateChangeHandler)(void* ctx, double sampleRate, int blockSize), void* ctx);
    void setBlockSize(std::string name, int blockSize);
    void unbindFromStreamMono(std::string name, dsp::stream<float>* stream);
    void unbindFromStreamStereo(std::string name, dsp::stream<dsp::stereo_t>* stream);
    std::string getNameFromVFO(std::string vfoName);
    void setSampleRate(std::string name, double sampleRate);
    void setAudioDevice(std::string name, int deviceId, double sampleRate);
    std::vector<std::string> getStreamNameList();
};

