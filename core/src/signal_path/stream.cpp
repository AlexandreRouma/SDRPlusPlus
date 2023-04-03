#include "stream.h"
#include <utils/flog.h>

void Sink::showMenu() {}

SinkEntry::SinkEntry(std::unique_ptr<Sink> sink) {
    this->sink = std::move(sink);
}

float SinkEntry::getVolume() {
    return volume;
}

void SinkEntry::setVolume(float volume) {
    this->volume = volume;
    volumeAdjust.setVolume(volume);
}

bool SinkEntry::getMuted() {
    return muted;
}

void SinkEntry::setMuted(bool muted) {
    this->muted = muted;
    volumeAdjust.setMuted(muted);
}

float SinkEntry::getPanning() {
    return panning;
}

void SinkEntry::setPanning(float panning) {
    this->panning = panning;
    // TODO: Update DSP
}

AudioStream::AudioStream(StreamManager* manager, const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate) {
    this->manager = manager;
    this->name = name;
    this->samplerate = samplerate;

    split.init(stream);
}

void AudioStream::setInput(dsp::stream<dsp::stereo_t>* stream) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    split.setInput(stream);
}

const std::string& AudioStream::getName() {
    return name;
}

void AudioStream::bindStream(dsp::stream<dsp::stereo_t>* stream) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    split.bindStream(stream);
}

void AudioStream::unbindStream(dsp::stream<dsp::stereo_t>* stream) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    split.unbindStream(stream);
}

int AudioStream::addSink(const std::string& type) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    std::lock_guard<std::recursive_mutex> lck2(manager->mtx);

    // Check that the sink provider is available
    if (manager->providers.find(type) == manager->providers.end()) {
        flog::error("Could not add sink of type '{}'. No provider is available for such a type.", type);
        return -1;
    }

    // Create sink instance
    int id = sinks.size();
    auto sink = manager->providers[type]->createSink(this);

    // Check that the sink was created succesfully
    if (!sink) {
        flog::error("Could not create sink of type '{}'. Provider returned null.", type);
        return -1;
    }

    // Create and save entry
    sinks.push_back(SinkEntry(std::move(sink)));

    return id;
}

void AudioStream::removeSink(int index) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    
    // Check that the index exists
    if (index >= sinks.size()) {
        flog::error("Can't remove sink of index {} from stream '{}'. Index out of range.", index, name);
        return;
    }

    // TODO: Free stuff

}

const std::vector<SinkEntry>& AudioStream::getSinks() {
    return sinks;
}

std::shared_ptr<AudioStream> StreamManager::registerStream(const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate) {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    
    // Check that an audio stream that name doesn't already exist
    if (streams.find(name) != streams.end()) {
        flog::error("Could not register audio stream with name '{}', a stream with this name already exists.", name);
        return NULL;
    }

    // Create stream and add to list
    streams[name] = std::make_shared<AudioStream>(this, stream, samplerate);
}

void StreamManager::unregisterStream(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Check that stream exists
    auto it = streams.find(name);
    if (it == streams.end()) {
        flog::error("Could not unregister audio stream with name '{}', no stream exists with that name.", name);
        return;
    }

    // Remove from list
    streams.erase(it);
}

const std::map<std::string, std::shared_ptr<AudioStream>>& StreamManager::getStreams() {
    return streams;
}