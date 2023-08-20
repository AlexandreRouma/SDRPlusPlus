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
#include <stdexcept>

class AudioStream;

using SinkID = int;

class SinkEntry;

class Sink {
public:
    Sink(SinkEntry* entry, dsp::stream<dsp::stereo_t>* stream, const std::string& name, SinkID id, const std::string& stringId);
    virtual ~Sink() {}

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void showMenu();

protected:
    SinkEntry* const entry;
    dsp::stream<dsp::stereo_t>* const stream;
    const std::string streamName;
    const SinkID id;
    const std::string stringId;
};

class SinkProvider {
    friend Sink;
public:
    /**
     * Create a sink instance.
     * @param name Name of the audio stream.
     * @param index Index of the sink in the menu. Should be use to keep settings.
    */
    virtual std::unique_ptr<Sink> createSink(SinkEntry* entry, dsp::stream<dsp::stereo_t>* stream, const std::string& name, SinkID id, const std::string& stringId) = 0;

    /**
     * Destroy a sink instance. This function is so that the provide knows at all times how many instances there are.
     * @param sink Instance of the sink.
    */
    virtual void destroySink(std::unique_ptr<Sink> sink) {
        sink.reset();
    }
};

class SinkEntryCreateException : public std::runtime_error {
public:
    SinkEntryCreateException(const char* what) : std::runtime_error(what) {}
};

class StreamManager;

// TODO: Would be cool to have data and audio sinks instead of just audio.
class SinkEntry {
    friend AudioStream;
    friend Sink;
public:
    SinkEntry(StreamManager* manager, AudioStream* parentStream, const std::string& type, SinkID id, double inputSamplerate);
    
    /**
     * Get the type of the sink.
     * @return Type of the sink.
    */
    std::string getType() const;

    /**
     * Change the type of the sink.
     * @param type New sink type.
    */
    void setType(const std::string& type);

    /**
     * Get the ID of the sink.
     * @return ID of the sink.
    */
    SinkID getID() const;

    /**
     * Get sink volume.
     * @return Volume as value between 0.0 and 1.0.
    */
    float getVolume() const;

    /**
     * Set sink volume.
     * @param volume Volume as value between 0.0 and 1.0.
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
     * @return Panning as value between -1.0 and 1.0 meaning panning to the left and right respectively.
    */
    float getPanning() const;

    /**
     * Set sink panning.
     * @param panning Panning as value between -1.0 and 1.0 meaning panning to the left and right respectively.
    */
    void setPanning(float panning);

    /**
     * Show the sink type-specific menu.
    */
    void showMenu();

    /**
     * Get the string form ID unique to both the sink and stream. Be used to reference settings.
     * @return Unique string ID.
    */
    std::string getStringID() const;

    // Emitted when the type of the sink was changed
    NewEvent<const std::string&> onTypeChanged;
    // Emmited when volume of the sink was changed
    NewEvent<float> onVolumeChanged;
    // Emitted when the muted state of the sink was changed
    NewEvent<bool> onMutedChanged;
    // Emitted when the panning of the sink was changed
    NewEvent<float> onPanningChanged;

    // TODO: Need to allow the sink to change the entry samplerate and start/stop the DSP
    //       This will also require allowing it to get a lock on the sink so others don't attempt to mess with it.
    std::lock_guard<std::recursive_mutex> getLock() const;
    void startDSP();
    void stopDSP();
    void setSamplerate(double samplerate);

private:
    void startSink();
    void stopSink();
    
    void destroy(bool forgetSettings);
    void setInputSamplerate(double samplerate);

    mutable std::recursive_mutex mtx;
    dsp::stream<dsp::stereo_t> input;
    dsp::multirate::RationalResampler<dsp::stereo_t> resamp;
    dsp::audio::Volume volumeAdjust;

    SinkProvider* provider = NULL;
    std::unique_ptr<Sink> sink;
    std::string type;
    const SinkID id;
    double inputSamplerate;
    AudioStream* const parentStream;
    StreamManager* const manager;

    std::string stringId;
    
    float volume = 1.0f;
    bool muted = false;
    float panning = 0.0f;
};

class StreamManager;

class AudioStream {
    friend StreamManager;
public:
    AudioStream(StreamManager* manager, const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate);
    ~AudioStream();

    /**
     * Set DSP stream input.
     * @param stream DSP stream.
     * @param samplerate New samplerate (optional, 0.0 if not used).
    */
    void setInput(dsp::stream<dsp::stereo_t>* stream, double samplerate = 0.0);

    // TODO: There must be a way to pre-stop things to avoid having weird shit happen

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
     * @param id ID of the sink. Optional, -1 if automatic.
     * @return ID of the new sink or -1 on error.
    */
    SinkID addSink(const std::string& type, SinkID id = -1);

    /**
     * Remove a sink from a stream.
     * @param id ID of the sink.
     * @param forgetSettings Forget the settings for the sink.
    */
    void removeSink(SinkID id, bool forgetSettings = true);

    /**
     * Aquire a lock for the sink list.
     * @return Shared lock for the sink list.
    */
    std::shared_lock<std::shared_mutex> getSinksLock();

    /**
     * Get the list of all sinks belonging to this stream.
     * @return Sink list.
    */
    const std::map<SinkID, std::shared_ptr<SinkEntry>>& getSinks() const;

    // TODO: This should only be callable by the module that created the stream

    /**
     * Start the DSP.
    */
    void startDSP();

    /**
     * Stop the DSP.
    */
    void stopDSP();

    // Emitted when the samplerate of the stream was changed
    NewEvent<double> onSamplerateChanged;
    // Emitted when a sink was added
    NewEvent<std::shared_ptr<SinkEntry>> onSinkAdded;
    // Emitted when a sink is being removed
    NewEvent<std::shared_ptr<SinkEntry>> onSinkRemove;

private:
    StreamManager* const manager;
    const std::string name;
    double samplerate;
    dsp::routing::Splitter<dsp::stereo_t> split;
    bool running = false;

    std::map<SinkID, std::shared_ptr<SinkEntry>> sinks;
    std::shared_mutex sinksMtx;
};

class StreamManager {
    friend SinkEntry;
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
    std::shared_lock<std::shared_mutex> getStreamsLock();

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
     * @return List of sink type names in alphabetical order.
    */
    const std::vector<std::string>& getSinkTypes() const;

    // Emitted when a stream was created
    NewEvent<std::shared_ptr<AudioStream>> onStreamCreated;
    // Emitted when a stream is about to be destroyed
    NewEvent<std::shared_ptr<AudioStream>> onStreamDestroy;
    // Emitted when a sink provider was registered
    NewEvent<const std::string&> onSinkProviderRegistered;
    // Emitted when a sink provider is about to be unregistered
    NewEvent<const std::string&> onSinkProviderUnregister;

private:
    std::map<std::string, std::shared_ptr<AudioStream>> streams;
    std::shared_mutex streamsMtx;

    std::map<std::string, SinkProvider*> providers;
    std::vector<std::string> sinkTypes;
    std::shared_mutex providersMtx;
};