#include <signal_path/sink.h>
#include <spdlog/spdlog.h>
#include <imgui/imgui.h>
#include <core.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SinkManager::SinkManager() {
    SinkManager::SinkProvider prov;
    prov.create = SinkManager::NullSink::create;
    registerSinkProvider("None", prov);
}

SinkManager::Stream::Stream(dsp::stream<dsp::stereo_t>* in, const Event<float>::EventHandler& srChangeHandler, float sampleRate) {
    _in = in;
    srChange.bindHandler(srChangeHandler);
    _sampleRate = sampleRate;
    splitter.init(_in);
}

void SinkManager::Stream::start() {
    if (running) {
        return;
    }
    sink->start();
    running = true;
}

void SinkManager::Stream::stop() {
    if (!running) {
        return;
    }
    sink->stop();
    running = false;
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
    providerNames.push_back(name);
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

    stream->sink = provider.create(stream, name, provider.ctx);
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

void SinkManager::startStream(std::string name) {
    if (streams.find(name) == streams.end()) {
        spdlog::error("Cannot start stream '{0}', this stream doesn't exist", name);
        return;
    }
    streams[name]->start();
}

void SinkManager::stopStream(std::string name) {
    if (streams.find(name) == streams.end()) {
        spdlog::error("Cannot stop stream '{0}', this stream doesn't exist", name);
        return;
    }
    streams[name]->stop();
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

void SinkManager::setStreamSink(std::string name, std::string providerName) {

}

void SinkManager::showMenu() {
    float menuWidth = ImGui::GetContentRegionAvailWidth();
    int count = 0;
    int maxCount = streams.size();
    
    std::string provStr = "";
    for (auto const& [name, provider] : providers) {
        provStr += name;
        provStr += '\0';
    }

    for (auto const& [name, stream] : streams) {
        ImGui::SetCursorPosX((menuWidth / 2.0f) - (ImGui::CalcTextSize(name.c_str()).x / 2.0f));
        ImGui::Text("%s", name.c_str());

        if (ImGui::Combo(CONCAT("##_sdrpp_sink_select_", name), &stream->providerId, provStr.c_str())) {
            if (stream->running) {
                stream->sink->stop();
            }
            delete stream->sink;
            SinkManager::SinkProvider prov = providers[providerNames[stream->providerId]];
            stream->sink = prov.create(stream, name, prov.ctx);
            if (stream->running) {
                stream->sink->start();
            }
        }

        stream->sink->menuHandler();

        ImGui::PopItemWidth();
        count++;
        if (count < maxCount) {
            ImGui::Spacing();
            ImGui::Separator();
        }
        ImGui::Spacing();
    }
}