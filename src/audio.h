#pragma once
#include <dsp/stream.h>
#include <dsp/routing.h>
#include <io/audio.h>
#include <map>
#include <watcher.h>

namespace audio {
    enum {
        STREAM_TYPE_MONO,
        STREAM_TYPE_STEREO,
        _STREAM_TYPE_COUNT
    };

    struct BoundStream_t {
        dsp::stream<float>* monoStream;
        dsp::stream<dsp::StereoFloat_t>* stereoStream;
        dsp::StereoToMono* s2m;
        dsp::MonoToStereo* m2s;
        void (*streamRemovedHandler)(void* ctx);
        void (*sampleRateChangeHandler)(void* ctx, float sampleRate, int blockSize);
        void* ctx;
        int type;
    };

    struct AudioStream_t {
        io::AudioSink* audio;
        dsp::stream<float>* monoAudioStream;
        dsp::stream<dsp::StereoFloat_t>* stereoAudioStream;
        std::vector<BoundStream_t> boundStreams;
        dsp::stream<float>* monoStream;
        dsp::DynamicSplitter<float>* monoDynSplit;
        dsp::stream<dsp::StereoFloat_t>* stereoStream;
        dsp::DynamicSplitter<dsp::StereoFloat_t>* stereoDynSplit;
        int (*sampleRateChangeHandler)(void* ctx, float sampleRate);
        float sampleRate;
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

    float registerMonoStream(dsp::stream<float>* stream, std::string name, std::string vfoName, int (*sampleRateChangeHandler)(void* ctx, float sampleRate), void* ctx);
    float registerStereoStream(dsp::stream<dsp::StereoFloat_t>* stream, std::string name, std::string vfoName, int (*sampleRateChangeHandler)(void* ctx, float sampleRate), void* ctx);
    void startStream(std::string name);
    void stopStream(std::string name);
    void removeStream(std::string name);
    dsp::stream<float>* bindToStreamMono(std::string name, void (*streamRemovedHandler)(void* ctx), void (*sampleRateChangeHandler)(void* ctx, float sampleRate, int blockSize), void* ctx);
    dsp::stream<dsp::StereoFloat_t>* bindToStreamStereo(std::string name, void (*streamRemovedHandler)(void* ctx), void (*sampleRateChangeHandler)(void* ctx, float sampleRate, int blockSize), void* ctx);
    void setBlockSize(std::string name, int blockSize);
    void unbindFromStreamMono(std::string name, dsp::stream<float>* stream);
    void unbindFromStreamStereo(std::string name, dsp::stream<dsp::StereoFloat_t>* stream);
    std::string getNameFromVFO(std::string vfoName);
    void setSampleRate(std::string name, float sampleRate);
    void setAudioDevice(std::string name, int deviceId, float sampleRate);
};

