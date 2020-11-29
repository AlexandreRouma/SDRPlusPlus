#pragma once
#include <map>
#include <string>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <dsp/routing.h>
#include <dsp/sink.h>
#include <mutex>
#include <event.h>
#include <vector>

class SinkManager {
public:
    SinkManager();

    class Sink {
    public:
        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void menuHandler() = 0;
    };

    class Stream {
    public:
        Stream(dsp::stream<dsp::stereo_t>* in, const Event<float>::EventHandler& srChangeHandler, float sampleRate);

        void start();
        void stop();

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
        int providerId = 0;
        bool running = false;
    };

    struct SinkProvider {
        SinkManager::Sink* (*create)(SinkManager::Stream* stream, std::string streamName, void* ctx);
        void* ctx;
    };

    class NullSink : SinkManager::Sink {
    public:
        void start() {}
        void stop() {}
        void menuHandler() {}

        static SinkManager::Sink* create(SinkManager::Stream* stream, std::string streamName, void* ctx) {
            return new SinkManager::NullSink;
        }
    };

    void registerSinkProvider(std::string name, SinkProvider provider);

    void registerStream(std::string name, Stream* stream);
    void unregisterStream(std::string name);

    void startStream(std::string name);
    void stopStream(std::string name);

    void setStreamSink(std::string name, std::string providerName);

    dsp::stream<dsp::stereo_t>* bindStream(std::string name);
    void unbindStream(std::string name, dsp::stream<dsp::stereo_t>* stream);

    void showMenu();

private:
    std::map<std::string, SinkProvider> providers;
    std::map<std::string, Stream*> streams;
    std::vector<std::string> providerNames;

};