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
    ~SinkEntry();

    float getVolume();
    void setVolume(float volume);

    bool getMuted();
    void setMuted(bool muted);

    float getPanning();
    void setPanning(float panning);
private:
    dsp::stream<dsp::stereo_t> stream;
    dsp::audio::Volume volumeAdjust;
    dsp::multirate::RationalResampler<dsp::stereo_t> resamp;

    std::unique_ptr<Sink> sink;
    float volume = 1.0f;
    bool muted = false;
    float panning = 0.0f;
};

class StreamManager;

class AudioStream {
    friend StreamManager;
public:
    AudioStream(StreamManager* manager, const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate);

    /**
     * Set DSP stream input.
     * @param stream DSP stream.
    */
    void setInput(dsp::stream<dsp::stereo_t>* stream);

    /**
     * Get the name of the stream.
     * @return Name of the stream.
    */
    const std::string& getName();

    // TODO: Maybe instead we want to resample the data?
    void bindStream(dsp::stream<dsp::stereo_t>* stream);
    void unbindStream(dsp::stream<dsp::stereo_t>* stream);

    double getSamplerate();
    void setSamplerate(double samplerate);

    /**
     * Add a sink to the stream.
     * @param type Type of the sink.
     * @return Index of the new sink or -1 on error.
    */
    int addSink(const std::string& type);

    /**
     * Change the type of a sink.
     * @param index Index of the sink.
     * @param type New sink type.
    */
    void setSinkType(int index, const std::string& type);

    /**
     * Remove a sink from a stream.
     * @param index Index of the sink.
    */
    void removeSink(int index);

    /**
     * Get the list of all sinks belonging to this stream.
     * @return Sink list.
    */
    const std::vector<std::shared_ptr<SinkEntry>>& getSinks();

private:

    std::recursive_mutex mtx;
    StreamManager* manager;
    std::string name;
    double samplerate;
    dsp::routing::Splitter<dsp::stereo_t> split;
    std::vector<std::shared_ptr<SinkEntry>> sinks;
};

class SinkProvider {
public:
    /**
     * Create a sink instance.
     * @param name Name of the audio stream.
     * @param index Index of the sink in the menu. Should be use to keep settings.
    */
    std::unique_ptr<Sink> createSink(const std::string& name, int index);

    /**
     * Destroy a sink instance. This function is so that the provide knows at all times how many instances there are.
     * @param sink Instance of the sink.
    */
    void destroySink(std::unique_ptr<Sink> sink);
};

class StreamManager {
    friend AudioStream;
public:
    /**
     * Create an audio stream.
     * @param name Name of the stream.
     * @param stream DSP stream that outputs the audio.
     * @param samplerate Samplerate of the audio data.
     * @return Audio stream instance.
    */
    std::shared_ptr<AudioStream> createStream(const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate);

    /**
     * Destroy an audio stream.
     * @param name Name of the stream.
    */
    void destroyStream(const std::string& name);

    /**
     * Register a sink provider.
     * @param name Name of the sink type.
     * @param provider Sink provider instance.
    */
    void registerSinkProvider(const std::string& name, SinkProvider* provider);

    /**
     * Unregister a sink provider.
     * @param name Name of the sink type.
    */
    void unregisterSinkProvider(const std::string& name);

    // TODO: Need a way to lock the list
    /**
     * Get a list of streams and their associated names.
     * @return Map of names to stream instance.
    */
    const std::map<std::string, std::shared_ptr<AudioStream>>& getStreams();

private:
    std::recursive_mutex mtx;
    std::map<std::string, std::shared_ptr<AudioStream>> streams;
    std::map<std::string, SinkProvider*> providers;
};