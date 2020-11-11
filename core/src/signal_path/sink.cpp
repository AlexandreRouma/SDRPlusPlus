#include <signal_path/sink.h>
#include <spdlog/spdlog.h>
#include <core.h>

SinkManager::SinkManager() {
    
}

SinkManager::Stream::Stream(dsp::stream<dsp::stereo_t>* in, const Event<float>::EventHandler& srChangeHandler, float sampleRate) {
    _in = in;
    srChange.bindHandler(srChangeHandler);
    _sampleRate = sampleRate;
    splitter.init(_in);
}

void SinkManager::Stream::setInput(dsp::stream<dsp::stereo_t>* in) {
    std::lock_guard<std::mutex> lck(ctrlMtx);
    _in = in;
    splitter.setInput(_in);
}

dsp::stream<dsp::stereo_t>* SinkManager::Stream::bindStream() {
    dsp::stream<dsp::stereo_t>* stream = new dsp::stream<dsp::stereo_t>;
    splitter.bindStream(stream);
    return stream;
}

void SinkManager::Stream::unbindStream(dsp::stream<dsp::stereo_t>* stream) {
    splitter.unbindStream(stream);
    delete stream;
}

void SinkManager::Stream::setSampleRate(float sampleRate) {
    std::lock_guard<std::mutex> lck(ctrlMtx);
    _sampleRate = sampleRate;
    srChange.emit(sampleRate);
}

void SinkManager::registerSinkProvider(std::string name, SinkProvider provider) {
    if (providers.find(name) != providers.end()) {
        spdlog::error("Cannot create sink provider '{0}', this name is already taken", name);
        return;
    }
    providers[name] = provider;
}

void SinkManager::registerStream(std::string name, SinkManager::Stream* stream) {
    if (streams.find(name) != streams.end()) {
        spdlog::error("Cannot register stream '{0}', this name is already taken", name);
        return;
    }
    
    core::configManager.aquire();
    std::string providerName = core::configManager.conf["defaultSink"];
    core::configManager.release();

    SinkManager::SinkProvider provider;
    if (providers.find(providerName) == providers.end()) {
        // TODO: get default
    }
    else {
        provider = providers[providerName];
    }

    stream->sink = provider.create(stream, provider.ctx);
}

void SinkManager::unregisterStream(std::string name) {
    if (streams.find(name) == streams.end()) {
        spdlog::error("Cannot unregister stream '{0}', this stream doesn't exist", name);
        return;
    }
    SinkManager::Stream* stream = streams[name];
    delete stream->sink;
    delete stream;
}

dsp::stream<dsp::stereo_t>* SinkManager::bindStream(std::string name) {
    if (streams.find(name) == streams.end()) {
        spdlog::error("Cannot bind to stream '{0}'. Stream doesn't exist", name);
        return NULL;
    }
    return streams[name]->bindStream();
}

void SinkManager::unbindStream(std::string name, dsp::stream<dsp::stereo_t>* stream) {
    if (streams.find(name) == streams.end()) {
        spdlog::error("Cannot unbind from stream '{0}'. Stream doesn't exist", name);
        return;
    }
    streams[name]->unbindStream(stream);
}

void SinkManager::showMenu() {
    for (auto const& [name, stream] : streams) {
        
    }
}