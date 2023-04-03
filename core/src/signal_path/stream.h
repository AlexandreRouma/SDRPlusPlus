#pragma once
#include <memory>
#include <vector>
#include <map>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <dsp/routing/splitter.h>
#include <dsp/multirate/rational_resampler.h>
#include <dsp/audio/volume.h>

class AudioStream;

class Sink {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void showMenu();

private:
    dsp::stream<dsp::stereo_t>* stream;
    AudioStream* audioStream = NULL;
};

class SinkEntry {
public:
    SinkEntry(std::unique_ptr<Sink> sink);

    float getVolume();
    void setVolume(float volume);

    bool getMuted();
    void setMuted(bool muted);

    float getPanning();
    void setPanning(float panning);
private:
    dsp::audio::Volume volumeAdjust;
    dsp::multirate::RationalResampler<dsp::stereo_t> resamp;

    std::unique_ptr<Sink> sink;
    float volume = 1.0f;
    bool muted = false;
    float panning = 0.0f;
};

class StreamManager;

class AudioStream {
    friend Sink;
    friend StreamManager;
public:
    AudioStream(StreamManager* manager, const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate);

    void setInput(dsp::stream<dsp::stereo_t>* stream);

    const std::string& getName();

    void bindStream(dsp::stream<dsp::stereo_t>* stream);
    void unbindStream(dsp::stream<dsp::stereo_t>* stream);

    double getSamplerate();
    void setSamplerate(double samplerate);

    int addSink(const std::string& type);
    void setSinkType(int index, const std::string& type);
    void removeSink(int index);
    const std::vector<SinkEntry>& getSinks();

private:
    void setSinkInputSamplerate(Sink* sink, double samplerate);

    std::recursive_mutex mtx;
    StreamManager* manager;
    std::string name;
    double samplerate;
    dsp::routing::Splitter<dsp::stereo_t> split;
    std::vector<SinkEntry> sinks;
};

class SinkProvider {
public:
    std::unique_ptr<Sink> createSink(const std::string& name, int index);
    void destroySink(std::unique_ptr<Sink> sink);
};

class StreamManager {
    friend AudioStream;
public:
    /**
     * Register an audio stream.
     * @param name Name of the stream.
     * @param stream DSP stream that outputs the audio.
     * @param samplerate Samplerate of the audio data.
     * @return Audio stream instance.
    */
    std::shared_ptr<AudioStream> registerStream(const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate);

    /**
     * Unregister an audio stream.
     * 
    */
    void unregisterStream(const std::string& name);

    void registerSinkProvider(const std::string& name, SinkProvider* provider);
    void unregisterSinkProvider(const std::string& name);

    const std::map<std::string, std::shared_ptr<AudioStream>>& getStreams();

private:
    std::recursive_mutex mtx;
    std::map<std::string, std::shared_ptr<AudioStream>> streams;
    std::map<std::string, SinkProvider*> providers;
};