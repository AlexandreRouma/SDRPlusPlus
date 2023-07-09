#include "stream.h"
#include <utils/flog.h>

Sink::Sink(SinkEntry* entry, dsp::stream<dsp::stereo_t>* stream,  const std::string& name, SinkID id) :
    entry(entry),
    stream(stream),
    streamName(name),
    id(id)
{}

void Sink::showMenu() {}

SinkEntry::SinkEntry(StreamManager* manager, AudioStream* parentStream, const std::string& type, SinkID id, double inputSamplerate) :
    manager(manager),
    parentStream(parentStream),
    id(id)
{
    this->type = type;
    this->inputSamplerate = inputSamplerate;
    
    // Initialize DSP
    resamp.init(&input, inputSamplerate, inputSamplerate);
    volumeAdjust.init(&resamp.out, 1.0f, false);

    // Initialize the sink
    setType(type);
}

std::string SinkEntry::getType() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    return type;
}

void SinkEntry::setType(const std::string& type) {
    // Get unique lock on the entry
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Delete existing sink
    if (sink) {
        provider->destroySink(std::move(sink));
    }

    // Get shared lock on sink types
    auto lck2 = manager->getSinkTypesLock();

    // Get the provider or throw error
    auto types = manager->getSinkTypes();
    if (std::find(types.begin(), types.end(), type) == types.end()) {
        this->type.clear();
        throw SinkEntryCreateException("Invalid sink type");
    }

    // Create sink
    this->type = type;
    provider = manager->providers[type];
    sink = provider->createSink(this, &volumeAdjust.out, parentStream->getName(), id);
}

SinkID SinkEntry::getID() const {
    return id;
}

float SinkEntry::getVolume() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    return volume;
}

void SinkEntry::setVolume(float volume) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    this->volume = volume;
    volumeAdjust.setVolume(volume);
    onVolumeChanged(volume);
}

bool SinkEntry::getMuted() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    return muted;
}

void SinkEntry::setMuted(bool muted) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    this->muted = muted;
    volumeAdjust.setMuted(muted);
    onMutedChanged(muted);
}

float SinkEntry::getPanning() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    return panning;
}

void SinkEntry::setPanning(float panning) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    this->panning = panning;
    // TODO
    onPanningChanged(panning);
}

void SinkEntry::showMenu() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    sink->showMenu();
}

void SinkEntry::startSink() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    sink->start();
}

void SinkEntry::stopSink() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    sink->stop();
}

void SinkEntry::startDSP() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    resamp.start();
    volumeAdjust.start();
}

void SinkEntry::stopDSP() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    resamp.stop();
    volumeAdjust.stop();
}

void SinkEntry::destroy(bool forgetSettings) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    if (sink) {
        provider->destroySink(std::move(sink));
    }
    type.clear();
}

void SinkEntry::setInputSamplerate(double samplerate) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    resamp.setInSamplerate(samplerate);
}

AudioStream::AudioStream(StreamManager* manager, const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate) :
    manager(manager),
    name(name)
{
    this->samplerate = samplerate;

    // Initialize DSP
    split.init(stream);
}

AudioStream::~AudioStream() {
    // Copy sink IDs
    std::vector<SinkID> ids;
    for (auto& [id, sink] : sinks) {
        ids.push_back(id);
    }

    // Remove them all
    for (auto& id : ids) {
        removeSink(id, false);
    }
}

void AudioStream::setInput(dsp::stream<dsp::stereo_t>* stream, double samplerate) {
    std::unique_lock<std::shared_mutex> lck(sinksMtx);

    // If all that's needed is to set the input, do it and return
    if (samplerate == 0.0) {
        split.setInput(stream);
        return;
    }

    // Update samplerate
    this->samplerate = samplerate;

    // Stop DSP
    if (running) {
        split.stop();
        for (auto& [id, sink] : sinks) {
            sink->stopDSP();
        }
    }


    // Set input and samplerate
    split.setInput(stream);
    for (auto& [id, sink] : sinks) {
        sink->setInputSamplerate(samplerate);
    }

    // Start DSP
    if (running) {
        for (auto& [id, sink] : sinks) {
            sink->startDSP();
        }
        split.start();
    }
}

void AudioStream::setSamplerate(double samplerate) {
    std::unique_lock<std::shared_mutex> lck(sinksMtx);

    // Update samplerate
    this->samplerate = samplerate;

    // Stop DSP
    if (running) {
        split.stop();
        for (auto& [id, sink] : sinks) {
            sink->stopDSP();
        }
    }

    // Set samplerate
    for (auto& [id, sink] : sinks) {
        sink->setInputSamplerate(samplerate);
    }

    // Start DSP
    if (running) {
        for (auto& [id, sink] : sinks) {
            sink->startDSP();
        }
        split.start();
    }
}

const std::string& AudioStream::getName() const {
    return name;
}

SinkID AudioStream::addSink(const std::string& type, SinkID id) {
    std::unique_lock<std::shared_mutex> lck(sinksMtx);

    // Find a free ID if not provided
    if (id < 0) {
        for (id = 0; sinks.find(id) != sinks.end(); id++);
    }
    else {
        // Check that the provided ID is valid
        if (sinks.find(id) != sinks.end()) {
            flog::error("Tried to create sink for stream '{}' with existing ID: {}", name, id);
            return -1;
        }
    }
    
    // Create sink entry
    std::shared_ptr<SinkEntry> sink;
    try {
        sink = std::make_shared<SinkEntry>(manager, this, type, id, samplerate);
    }
    catch (SinkEntryCreateException e) {
        flog::error("Tried to create sink for stream '{}' with ID '{}': {}", name, id, e.what());
        return -1;
    }

    // Start the sink and DSP
    sink->startSink();
    if (running) { sink->startDSP(); }

    // Bind the sinks's input
    split.bindStream(&sink->input);

    // Add sink to list
    sinks[id] = sink;

    // Release lock and emit event
    lck.unlock();
    onSinkAdded(sink);

    return id;
}

void AudioStream::removeSink(SinkID id, bool forgetSettings) {
    // Acquire shared lock
    std::shared_ptr<SinkEntry> sink;
    {
        std::shared_lock<std::shared_mutex> lck(sinksMtx);

        // Check that the ID exists
        if (sinks.find(id) == sinks.end()) {
            flog::error("Tried to remove sink with unknown ID: {}", id);
            return;
        }

        // Get sink
        sink = sinks[id];
    }
    
    // Emit event
    onSinkRemove(sink);

    // Acquire unique lock
    {
        std::unique_lock<std::shared_mutex> lck(sinksMtx);

        // Check that it's still in the list
        if (sinks.find(id) == sinks.end()) {
            flog::error("Tried to remove sink with unknown ID: {}", id);
            return;
        }

        // Remove from list
        sinks.erase(id);

        // Unbind the sink's steam
        split.unbindStream(&sink->input);
        
        // Stop the sink and DSP
        sink->stopDSP();
        sink->stopSink();

        // Delete instance
        sink->destroy(forgetSettings);
    }
}

std::shared_lock<std::shared_mutex> AudioStream::getSinksLock() {
    return std::shared_lock<std::shared_mutex>(sinksMtx);
}

const std::map<SinkID, std::shared_ptr<SinkEntry>>& AudioStream::getSinks() const {
    return sinks;
}

void AudioStream::startDSP() {
    // TODO: Maybe add a different mutex for the stream?
    std::unique_lock<std::shared_mutex> lck(sinksMtx);

    // Check if already running
    if (running) { return; }

    // Start all DSP
    split.start();
    for (auto& [id, sink] : sinks) {
        sink->startDSP();
    }
    running = true;
}

void AudioStream::stopDSP() {
    // TODO: Maybe add a different mutex for the stream?
    std::unique_lock<std::shared_mutex> lck(sinksMtx);

    // Check if already running
    if (!running) { return; }

    // Start all DSP
    split.stop();
    for (auto& [id, sink] : sinks) {
        sink->stopDSP();
    }
    running = false;
}

std::shared_ptr<AudioStream> StreamManager::createStream(const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate) {
    std::unique_lock<std::shared_mutex> lck(streamsMtx);

    // Check that no stream with that name already exists
    if (streams.find(name) != streams.end()) {
        flog::error("Tried to created stream with an existing name: {}", name);
        return NULL;
    }

    // Create and save stream
    auto newStream = std::make_shared<AudioStream>(this, name, stream, samplerate);
    streams[name] = newStream;

    // Release lock and emit event
    lck.unlock();
    onStreamCreated(newStream);

    return newStream;
}

void StreamManager::destroyStream(std::shared_ptr<AudioStream>& stream) {
    // Emit event
    onStreamDestroy(stream);

    // Aquire complete lock on the stream list
    {
        std::unique_lock<std::shared_mutex> lck(streamsMtx);

        // Get iterator of the stream
        auto it = std::find_if(streams.begin(), streams.end(), [&stream](std::pair<const std::string, std::shared_ptr<AudioStream>> e) {
            return e.second == stream;
        });
        if (it == streams.end()) {
            flog::error("Tried to delete a stream using an invalid pointer. Stream not found in list");
            return;
        }

        // Delete entry from list
        flog::debug("Stream pointer uses, should be 2 and is {}", (int)stream.use_count());
        streams.erase(it);
    }

    // Reset passed pointer
    stream.reset();
}

std::shared_lock<std::shared_mutex> StreamManager::getStreamsLock() {
    return std::shared_lock<std::shared_mutex>(streamsMtx);
}

const std::map<std::string, std::shared_ptr<AudioStream>>& StreamManager::getStreams() const {
    return streams;
}

void StreamManager::registerSinkProvider(const std::string& name, SinkProvider* provider) {
    std::unique_lock<std::shared_mutex> lck(providersMtx);

    // Check that a provider with that name doesn't already exist
    if (providers.find(name) != providers.end()) {
        flog::error("Tried to register a sink provider with an existing name: {}", name);
        return;
    }

    // Add provider to the list and sort name list
    providers[name] = provider;
    sinkTypes.push_back(name);
    std::sort(sinkTypes.begin(), sinkTypes.end());

    // Release lock and emit event
    lck.unlock();
    onSinkProviderRegistered(name);
}

void StreamManager::unregisterSinkProvider(SinkProvider* provider) {
    // Get provider name for event
    std::string type;
    {
        std::shared_lock<std::shared_mutex> lck(providersMtx);
        auto it = std::find_if(providers.begin(), providers.end(), [&provider](std::pair<const std::string, SinkProvider *> e) {
            return e.second == provider;
        });
        if (it == providers.end()) {
            flog::error("Tried to unregister sink provider using invalid pointer");
            return;
        }
        type = (*it).first;
    }

    // Emit event
    onSinkProviderUnregister(type);

    // Acquire shared lock on streams
    {
        std::unique_lock<std::shared_mutex> lck1(providersMtx);
        std::shared_lock<std::shared_mutex> lck2(streamsMtx);
        for (auto& [name, stream] : streams) {
            // Aquire lock on sink list
            auto sLock = stream->getSinksLock();
            auto sinks = stream->getSinks();

            // Find all sinks with the type that is about to be removed
            std::vector<SinkID> toRemove;
            for (auto& [id, sink] : sinks) {
                if (sink->getType() != type) { continue; }
                toRemove.push_back(id);
            }

            // Remove them all (TODO: THERE IS RACE CONDITION IF A SINK IS CHANGED AFTER LISTING)
            sLock.unlock();
            for (auto& id : toRemove) {
                stream->removeSink(id);
            }
        }

        // Remove from the lists
        if (providers.find(type) != providers.end()) {
            providers.erase(type);
        }
        else {
            flog::error("Could not remove sink provider from list");
        }

        auto it = std::find(sinkTypes.begin(), sinkTypes.end(), type);
        if (it != sinkTypes.end()) {
            sinkTypes.erase(it);
        }
        else {
            flog::error("Could not remove sink provider from sink type list");
        }
    }
}

std::shared_lock<std::shared_mutex> StreamManager::getSinkTypesLock() {
    return std::shared_lock<std::shared_mutex>(providersMtx);
}

const std::vector<std::string>& StreamManager::getSinkTypes() const {
    // TODO: This allows code to modify the names...
    return sinkTypes;
}