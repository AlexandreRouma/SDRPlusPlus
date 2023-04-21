#pragma once
#include <memory>
#include <vector>
#include <map>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <dsp/routing/splitter.h>
#include <dsp/multirate/rational_resampler.h>
#include <dsp/audio/volume.h>
#include <utils/new_event.h>
#include <shared_mutex>

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
    friend AudioStream;
    SinkEntry(dsp::stream<dsp::stereo_t>* stream, const std::string& type, int index);
public:
    ~SinkEntry();

    /**
     * Change the type of the sink.
     * @param type New sink type.
    */
    void setType(const std::string& type);

    /**
     * Get sink volume.
     * @return Volume as value between 0 and 1.
    */
    float getVolume() const;

    /**
     * Set sink volume.
     * @param volume Volume as value between 0 and 1.
    */
    void setVolume(float volume);

    /**
     * Check if the sink is muted.
     * @return True if muted, false if not.
    */
    bool getMuted() const;

    /**
     * Set wether or not the sink is muted
     * @param muted True to mute, false to unmute.
    */
    void setMuted(bool muted);

    /**
     * Get sink panning.
     * @return Panning as value between -1 and 1 meaning panning to the left and right respectively.
    */
    float getPanning() const;

    /**
     * Set sink panning.
     * @param panning Panning as value between -1 and 1 meaning panning to the left and right respectively.
    */
    void setPanning(float panning);

private:
    dsp::audio::Volume volumeAdjust;

    std::unique_ptr<Sink> sink;
    const int index;
    float volume = 1.0f;
    bool muted = false;
    float panning = 0.0f;
};

class StreamManager;

class AudioStream {
    friend StreamManager;
    AudioStream(StreamManager* manager, const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate);
public:

    /**
     * Set DSP stream input.
     * @param stream DSP stream.
    */
    void setInput(dsp::stream<dsp::stereo_t>* stream);

    /**
     * Set the samplerate of the input stream.
     * @param samplerate Samplerate in Hz.
    */
    void setSamplerate(double samplerate);

    /**
     * Get the name of the stream.
     * @return Name of the stream.
    */
    const std::string& getName() const;

    /**
     * Add a sink to the stream.
     * @param type Type of the sink.
     * @return Index of the new sink or -1 on error.
    */
    int addSink(const std::string& type);

    /**
     * Remove a sink from a stream.
     * @param index Index of the sink.
    */
    void removeSink(int index);

    /**
     * Aquire a lock for the sink list.
     * @return Shared lock for the sink list.
    */
    std::shared_lock<std::shared_mutex> getSinksLock();

    /**
     * Get the list of all sinks belonging to this stream.
     * @return Sink list.
    */
    const std::vector<std::shared_ptr<SinkEntry>>& getSinks() const;

private:
    std::recursive_mutex mtx;
    StreamManager* manager;
    std::string name;
    double samplerate;
    dsp::routing::Splitter<dsp::stereo_t> split;

    std::vector<std::shared_ptr<SinkEntry>> sinks;
    std::shared_mutex sinksMtx;
};

class SinkProvider {
public:
    /**
     * Create a sink instance.
     * @param name Name of the audio stream.
     * @param index Index of the sink in the menu. Should be use to keep settings.
    */
    virtual std::unique_ptr<Sink> createSink(dsp::stream<dsp::stereo_t>* stream, const std::string& name, int index) = 0;

    /**
     * Destroy a sink instance. This function is so that the provide knows at all times how many instances there are.
     * @param sink Instance of the sink.
    */
    virtual void destroySink(std::unique_ptr<Sink> sink) = 0;
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
     * @param stream Stream to destroy. The passed shared pointer will be automatically reset.
    */
    void destroyStream(std::shared_ptr<AudioStream>& stream);

    /**
     * Aquire a lock for the stream list.
     * @return Shared lock for the stream list.
    */
    std::shared_lock<std::shared_mutex> getStreamsLock() {
        return std::shared_lock<std::shared_mutex>(streamsMtx);
    }

    /**
     * Get a list of streams and their associated names.
     * @return Map of names to stream instance.
    */
    const std::map<std::string, std::shared_ptr<AudioStream>>& getStreams() const;

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
    void unregisterSinkProvider(SinkProvider* provider);

    /**
     * Aquire a lock for the sink type list.
     * @return Shared lock for the sink type list.
    */
    std::shared_lock<std::shared_mutex> getSinkTypesLock();

    /**
     * Get a list of sink types.
     * @return List of sink type names.
    */
    const std::vector<std::string>& getSinkTypes() const;

    // Emitted when a stream was created
    NewEvent<std::shared_ptr<AudioStream>> onStreamCreated;
    // Emitted when a stream is about to be destroyed
    NewEvent<std::shared_ptr<AudioStream>> onStreamDestroy;
    // Emitted when a sink provider was registered
    NewEvent<std::string> onSinkProviderRegistered;
    // Emitted when a sink provider is about to be unregistered
    NewEvent<std::string> onSinkProviderUnregister;

private:
    std::map<std::string, std::shared_ptr<AudioStream>> streams;
    std::shared_mutex streamsMtx;

    std::map<std::string, SinkProvider*> providers;
    std::vector<std::string> sinkTypes;
    std::shared_mutex providersMtx;
};