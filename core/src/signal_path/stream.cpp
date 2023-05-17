// #include "stream.h"
// #include <utils/flog.h>

// Sink::Sink(dsp::stream<dsp::stereo_t>* stream,  const std::string& name, SinkID id) :
//     stream(stream),
//     streamName(name),
//     id(id)
// {}

// void Sink::showMenu() {}

// AudioStream::AudioStream(StreamManager* manager, const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate) :
//     manager(manager),
//     name(name)
// {
//     this->samplerate = samplerate;

//     // Initialize DSP
//     split.init(stream);
//     split.start();
// }

// void AudioStream::setInput(dsp::stream<dsp::stereo_t>* stream, double samplerate = 0.0) {
//     std::lock_guard<std::recursive_mutex> lck1(mtx);

//     // If all that's needed is to set the input, do it and return
//     if (samplerate == 0.0) {
//         split.setInput(stream);
//         return;
//     }

//     // Lock sink list
//     {
//         std::unique_lock<std::shared_mutex> lck2(sinksMtx);
//         this->samplerate = samplerate;

//         // Stop DSP
//         split.stop();
//         for (auto& [id, sink] : sinks) {
//             sink->stopDSP();
//         }

//         // Set input and samplerate
//         split.setInput(stream);
//         for (auto& [id, sink] : sinks) {
//             sink->setInputSamplerate(samplerate);
//         }

//         // Start DSP
//         for (auto& [id, sink] : sinks) {
//             sink->startDSP();
//         }
//         split.start();
//     }
// }

// SinkID AudioStream::addSink(const std::string& type, SinkID id = -1) {
//     std::unique_lock<std::shared_mutex> lck(sinksMtx);

//     // Find a free ID if not provided
//     if (id < 0) {
//         for (id = 0;; id++) {
//             if (sinks.find(id) != sinks.end()) { continue; }
//         }
//     }
//     else {
//         // Check that the provided ID is valid
//         if (sinks.find(id) != sinks.end()) {
//             flog::error("Tried to create sink for stream '{}' with existing ID: {}", name, id);
//             return -1;
//         }
//     }
    
//     // Create sink entry
//     std::shared_ptr<SinkEntry> sink;
//     try {
//         sink = std::make_shared<SinkEntry>(type, id, samplerate);
//     }
//     catch (SinkEntryCreateException e) {
//         flog::error("Tried to create sink for stream '{}' with ID '{}': {}", name, id, e.what());
//         return -1;
//     }

//     // Start the sink and DSP
//     sink->startSink();
//     sink->startDSP();

//     // Bind the sinks's input
//     split.bindStream(&sink->input);

//     // Add sink to list
//     sinks[id] = sink;

//     // Release lock and emit event
//     lck.unlock();
//     onSinkAdded(sink);
// }

// void AudioStream::removeSink(SinkID id, bool forgetSettings = true) {
//     // Acquire shared lock
//     std::shared_ptr<SinkEntry> sink;
//     {
//         std::shared_lock<std::shared_mutex> lck(sinksMtx);

//         // Check that the ID exists
//         if (sinks.find(id) == sinks.end()) {
//             flog::error("Tried to remove sink with unknown ID: {}", id);
//             return;
//         }

//         // Get sink
//         sink = sinks[id];
//     }
    
//     // Emit event
//     onSinkRemove(sink);

//     // Acquire unique lock
//     {
//         std::unique_lock<std::shared_mutex> lck(sinksMtx);

//         // Check that it's still in the list
//         if (sinks.find(id) == sinks.end()) {
//             flog::error("Tried to remove sink with unknown ID: {}", id);
//             return;
//         }

//         // Remove from list
//         sinks.erase(id);

//         // Unbind the sink's steam
//         split.unbindStream(&sink->input);
        
//         // Stop the sink and DSP
//         sink->stopDSP();
//         sink->stopSink();
//     }
// }

// std::shared_lock<std::shared_mutex> AudioStream::getSinksLock() {
//     return std::shared_lock<std::shared_mutex>(sinksMtx);
// }

// const std::map<SinkID, std::shared_ptr<SinkEntry>>& AudioStream::getSinks() const {
//     return sinks;
// }

// std::shared_ptr<AudioStream> StreamManager::createStream(const std::string& name, dsp::stream<dsp::stereo_t>* stream, double samplerate) {
//     std::unique_lock<std::shared_mutex> lck(streamsMtx);

//     // Check that no stream with that name already exists
//     if (streams.find(name) != streams.end()) {
//         flog::error("Tried to created stream with an existing name: {}", name);
//         return NULL;
//     }

//     // Create and save stream
//     auto newStream = std::make_shared<AudioStream>(this, name, stream, samplerate);
//     streams[name] = newStream;

//     // Release lock and emit event
//     lck.unlock();
//     onStreamCreated(newStream);
// }

// void StreamManager::destroyStream(std::shared_ptr<AudioStream>& stream) {
//     // Emit event
//     onStreamDestroy(stream);

//     // Aquire complete lock on the stream list
//     {
//         std::unique_lock<std::shared_mutex> lck(streamsMtx);

//         // Get iterator of the stream
//         auto it = std::find_if(streams.begin(), streams.end(), [&stream](std::shared_ptr<AudioStream>& s) {
//             return s == stream;
//         });
//         if (it == streams.end()) {
//             flog::error("Tried to delete a stream using an invalid pointer. Stream not found in list");
//             return;
//         }

//         // Delete entry from list
//         flog::debug("Stream pointer uses, should be 2 and is {}", stream.use_count());
//         streams.erase(it);
//     }

//     // Reset passed pointer
//     stream.reset();
// }

// std::shared_lock<std::shared_mutex> StreamManager::getStreamsLock() {
//     return std::shared_lock<std::shared_mutex>(streamsMtx);
// }

// const std::map<std::string, std::shared_ptr<AudioStream>>& StreamManager::getStreams() const {
//     return streams;
// }

// void StreamManager::registerSinkProvider(const std::string& name, SinkProvider* provider) {
//     std::unique_lock<std::shared_mutex> lck(providersMtx);

//     // Check that a provider with that name doesn't already exist
//     if (providers.find(name) != providers.end()) {
//         flog::error("Tried to register a sink provider with an existing name: {}", name);
//         return;
//     }

//     // Add provider to the list and sort name list
//     providers[name] = provider;
//     sinkTypes.push_back(name);
//     std::sort(sinkTypes.begin(), sinkTypes.end());

//     // Release lock and emit event
//     lck.unlock();
//     onSinkProviderRegistered(name);
// }

// void StreamManager::unregisterSinkProvider(SinkProvider* provider) {
//     // Get provider name for event
//     std::string type;
//     {
//         std::shared_lock<std::shared_mutex> lck(providersMtx);
//         auto it = std::find_if(providers.begin(), providers.end(), [&provider](SinkProvider* p) {
//             p == provider;
//         });
//         if (it == providers.end()) {
//             flog::error("Tried to unregister sink provider using invalid pointer");
//             return;
//         }
//         type = (*it).first;
//     }

//     // Emit event
//     onSinkProviderUnregister(type);

//     // Acquire shared lock on streams
//     {
//         std::shared_lock<std::shared_mutex> lck(streamsMtx);
//         for (auto& [name, stream] : streams) {
//             std::vector<SinkID> toRemove;

//             // Aquire lock on sink list
//             auto sLock = stream->getSinksLock();
//             auto sinks = stream->getSinks();

//             // Find all sinks with the type that is about to be removed
//             for (auto& [id, sink] : sinks) {
//                 if (sink->getType() != type) { continue; }
//                 toRemove.push_back(id);
//             }

//             // Remove them all (TODO: THERE IS RACE CONDITION IF A SINK IS CHANGED AFTER LISTING)
//             for (auto& id : toRemove) {
//                 stream->removeSink(id);
//             }
//         }
//     }

//     // Remove from the lists
//     {
//         std::unique_lock<std::shared_mutex> lck(providersMtx);
//         if (providers.find(type) != providers.end()) {
//             providers.erase(type);
//         }
//         else {
//             flog::error("Could not remove sink provider from list");
//         }

//         auto it = std::find(sinkTypes.begin(), sinkTypes.end(), type);
//         if (it != sinkTypes.end()) {
//             sinkTypes.erase(it);
//         }
//         else {
//             flog::error("Could not remove sink provider from sink type list");
//         }
//     }
// }

// std::shared_lock<std::shared_mutex> StreamManager::getSinkTypesLock() {
//     return  std::shared_lock<std::shared_mutex>(providersMtx);
// }

// const std::vector<std::string>& StreamManager::getSinkTypes() const {
//     return sinkTypes;
// }