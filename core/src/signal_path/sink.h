#pragma once
#include <map>
#include <string>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <dsp/routing.h>
#include <dsp/processing.h>
#include <dsp/sink.h>
#include <mutex>
#include <event.h>
#include <vector>

class SinkManager {
public:
    SinkManager();

    class Sink {
    public:
        virtual ~Sink() {}
        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void menuHandler() = 0;
    };

    class Stream {
    public:
        Stream() {}
        Stream(dsp::stream<dsp::stereo_t>* in, const Event<float>::EventHandler& srChangeHandler, float sampleRate);

        void init(dsp::stream<dsp::stereo_t>* in, const Event<float>::EventHandler& srChangeHandler, float sampleRate);

        void start();
        void stop();

        void setVolume(float volume);
        float getVolume();

        void setSampleRate(float sampleRate);
        float getSampleRate();

        void setInput(dsp::stream<dsp::stereo_t>* in);

        dsp::stream<dsp::stereo_t>* bindStream();
        void unbindStream(dsp::stream<dsp::stereo_t>* stream);

        friend SinkManager;
        friend SinkManager::Sink;

        dsp::stream<dsp::stereo_t>* sinkOut;

        Event<float> srChange;        

    private:
        dsp::stream<dsp::stereo_t>* _in;
        dsp::Splitter<dsp::stereo_t> splitter;
        SinkManager::Sink* sink;
        dsp::stream<dsp::stereo_t> volumeInput;
        dsp::Volume<dsp::stereo_t> volumeAjust;
        std::mutex ctrlMtx;
        float _sampleRate;
        int providerId = 0;
        bool running = false;

        float guiVolume = 1.0f;
    };

    struct SinkProvider {
        SinkManager::Sink* (*create)(SinkManager::Stream* stream, std::string streamName, void* ctx);
        void* ctx;
    };

    class NullSink : SinkManager::Sink {
    public:
        NullSink(SinkManager::Stream* stream) {
            ns.init(stream->sinkOut);
        }
        void start() { ns.start(); }
        void stop() { ns.stop(); }
        void menuHandler() {}

        static SinkManager::Sink* create(SinkManager::Stream* stream, std::string streamName, void* ctx) {
            stream->srChange.emit(48000);
            return new SinkManager::NullSink(stream);
        }

    private:
        dsp::NullSink<dsp::stereo_t> ns;

    };

    void registerSinkProvider(std::string name, SinkProvider provider);

    void registerStream(std::string name, Stream* stream);
    void unregisterStream(std::string name);

    void startStream(std::string name);
    void stopStream(std::string name);

    float getStreamSampleRate(std::string name);

    void setStreamSink(std::string name, std::string providerName);

    void showVolumeSlider(std::string name, std::string prefix, float width, float btnHeight = -1.0f, int btwBorder = 0, bool sameLine = false);

    dsp::stream<dsp::stereo_t>* bindStream(std::string name);
    void unbindStream(std::string name, dsp::stream<dsp::stereo_t>* stream);

    void loadSinksFromConfig();
    void showMenu();

    std::vector<std::string> getStreamNames();

private:
    void loadStreamConfig(std::string name);
    void saveStreamConfig(std::string name);

    std::map<std::string, SinkProvider> providers;
    std::map<std::string, Stream*> streams;
    std::vector<std::string> providerNames;
    std::vector<std::string> streamNames;

};