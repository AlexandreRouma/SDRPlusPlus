#pragma once
#include <map>
#include <string>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <dsp/routing.h>
#include <mutex>
#include <event.h>

class SinkManager {
public:
    SinkManager();

    class Sink {
    public:
        virtual void menuHandler() = 0;
    };

    class Stream {
    public:
        Stream(dsp::stream<dsp::stereo_t>* in, const Event<float>::EventHandler& srChangeHandler, float sampleRate);

        void setInput(dsp::stream<dsp::stereo_t>* in);

        dsp::stream<dsp::stereo_t>* bindStream();
        void unbindStream(dsp::stream<dsp::stereo_t>* stream);

        friend SinkManager;
        friend SinkManager::Sink;

        Event<float> srChange;

    private:
        void setSampleRate(float sampleRate);

        dsp::stream<dsp::stereo_t>* _in;
        dsp::Splitter<dsp::stereo_t> splitter;
        SinkManager::Sink* sink;
        std::mutex ctrlMtx;
        float _sampleRate;

    };

    struct SinkProvider {
        SinkManager::Sink* (*create)(SinkManager::Stream* stream, void* ctx);
        void* ctx;
    };

    void registerSinkProvider(std::string name, SinkProvider provider);

    void registerStream(std::string name, Stream* stream);
    void unregisterStream(std::string name);

    dsp::stream<dsp::stereo_t>* bindStream(std::string name);
    void unbindStream(std::string name, dsp::stream<dsp::stereo_t>* stream);

    void showMenu();

private:
    // TODO: Replace with std::unordered_map
    std::map<std::string, SinkProvider> providers;
    std::map<std::string, Stream*> streams;


};