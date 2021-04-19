#include <signal_path/sink.h>
#include <spdlog/spdlog.h>
#include <imgui/imgui.h>
#include <gui/style.h>
#include <gui/icons.h>

#include <core.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SinkManager::SinkManager() {
    SinkManager::SinkProvider prov;
    prov.create = SinkManager::NullSink::create;
    registerSinkProvider("None", prov);
}

SinkManager::Stream::Stream(dsp::stream<dsp::stereo_t>* in, const EventHandler<float>& srChangeHandler, float sampleRate) {
    init(in, srChangeHandler, sampleRate);
}

void SinkManager::Stream::init(dsp::stream<dsp::stereo_t>* in, const EventHandler<float>& srChangeHandler, float sampleRate) {
    _in = in;
    srChange.bindHandler(srChangeHandler);
    _sampleRate = sampleRate;
    splitter.init(_in);
    splitter.bindStream(&volumeInput);
    volumeAjust.init(&volumeInput, 1.0f);
    sinkOut = &volumeAjust.out;
}

void SinkManager::Stream::start() {
    if (running) {
        return;
    }
    
    splitter.start();
    volumeAjust.start();
    sink->start();
    running = true;
}

void SinkManager::Stream::stop() {
    if (!running) {
        return;
    }
    splitter.stop();
    volumeAjust.stop();
    sink->stop();
    running = false;
}

void SinkManager::Stream::setVolume(float volume) {
    guiVolume = volume;
    volumeAjust.setVolume(volume);
}

float SinkManager::Stream::getVolume() {
    return guiVolume;
}

float SinkManager::Stream::getSampleRate() {
    return _sampleRate;
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

    SinkManager::SinkProvider provider;

    provider = providers["None"];

    stream->sink = provider.create(stream, name, provider.ctx);

    streams[name] = stream;
    streamNames.push_back(name);
}

void SinkManager::unregisterStream(std::string name) {
    if (streams.find(name) == streams.end()) {
        spdlog::error("Cannot unregister stream '{0}', this stream doesn't exist", name);
        return;
    }
    spdlog::error("unregisterStream NOT IMPLEMENTED!!!!!!!");
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

float SinkManager::getStreamSampleRate(std::string name) {
    if (streams.find(name) == streams.end()) {
        spdlog::error("Cannot get sample rate of stream '{0}', this stream doesn't exist", name);
        return -1.0f;
    }
    return streams[name]->getSampleRate();
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
    spdlog::warn("setStreamSink is NOT implemented!!!");
}

void SinkManager::showVolumeSlider(std::string name, std::string prefix, float width, float btnHeight, int btwBorder, bool sameLine) {
    // TODO: Replace map with some hashmap for it to be faster
    float height = ImGui::GetTextLineHeightWithSpacing() + 2;
    float sliderHeight = height;
    if (btnHeight > 0) {
        height = btnHeight;
    }

    float ypos = ImGui::GetCursorPosY();

    if (streams.find(name) == streams.end() || name == "") {
        float dummy = 0.0f;
        style::beginDisabled();
        ImGui::PushID(ImGui::GetID(("sdrpp_unmute_btn_" + name).c_str()));
        ImGui::ImageButton(icons::MUTED, ImVec2(height, height), ImVec2(0, 0), ImVec2(1, 1), btwBorder);
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(width - height - 8);
        ImGui::SetCursorPosY(ypos + ((height - sliderHeight) / 2.0f) + btwBorder);
        ImGui::SliderFloat((prefix + name).c_str(), &dummy, 0.0f, 1.0f, "");
        style::endDisabled();
        if (sameLine) { ImGui::SetCursorPosY(ypos); }
        return;
    }

    SinkManager::Stream* stream = streams[name];

    if (stream->volumeAjust.getMuted()) {
        ImGui::PushID(ImGui::GetID(("sdrpp_unmute_btn_" + name).c_str()));
        if (ImGui::ImageButton(icons::MUTED, ImVec2(height, height), ImVec2(0, 0), ImVec2(1, 1), btwBorder)) {
            stream->volumeAjust.setMuted(false);
            core::configManager.aquire();
            saveStreamConfig(name);
            core::configManager.release(true);
        }
        ImGui::PopID();
    }
    else {
        ImGui::PushID(ImGui::GetID(("sdrpp_mute_btn_" + name).c_str()));
        if (ImGui::ImageButton(icons::UNMUTED, ImVec2(height, height), ImVec2(0, 0), ImVec2(1, 1), btwBorder)) {
            stream->volumeAjust.setMuted(true);
            core::configManager.aquire();
            saveStreamConfig(name);
            core::configManager.release(true);
        }
        ImGui::PopID();
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(width - height - 8);
    ImGui::SetCursorPosY(ypos + ((height - sliderHeight) / 2.0f) + btwBorder);
    if (ImGui::SliderFloat((prefix + name).c_str(), &stream->guiVolume, 0.0f, 1.0f, "")) {
        stream->setVolume(stream->guiVolume);
        core::configManager.aquire();
        saveStreamConfig(name);
        core::configManager.release(true);
    }
    if (sameLine) { ImGui::SetCursorPosY(ypos); }
    //ImGui::SetCursorPosY(ypos);
}

void SinkManager::loadStreamConfig(std::string name) {
    json conf = core::configManager.conf["streams"][name];
    SinkManager::Stream* stream = streams[name];
    std::string provName = conf["sink"];
    if (providers.find(provName) == providers.end()) {
        provName = providerNames[0];
    }
    if (stream->running) {
        stream->sink->stop();
    }
    delete stream->sink;
    SinkManager::SinkProvider prov = providers[provName];
    stream->providerId = std::distance(providerNames.begin(), std::find(providerNames.begin(), providerNames.end(), provName));
    stream->sink = prov.create(stream, name, prov.ctx);
    if (stream->running) {
        stream->sink->start();
    }
    stream->setVolume(conf["volume"]);
    stream->volumeAjust.setMuted(conf["muted"]);
}

void SinkManager::saveStreamConfig(std::string name) {
    SinkManager::Stream* stream = streams[name];
    json conf;
    conf["sink"] = providerNames[stream->providerId];
    conf["volume"] = stream->getVolume();
    conf["muted"] = stream->volumeAjust.getMuted();
    core::configManager.conf["streams"][name] = conf;
}

// Note: aquire and release config before running this
void SinkManager::loadSinksFromConfig() {
    for (auto const& [name, stream] : streams) {
        if (!core::configManager.conf["streams"].contains(name)) { continue; }
        loadStreamConfig(name);
    }
}

void SinkManager::showMenu() {
    float menuWidth = ImGui::GetContentRegionAvailWidth();
    int count = 0;
    int maxCount = streams.size();
    
    std::string provStr = "";
    for (auto const& name : providerNames) {
        provStr += name;
        provStr += '\0';
    }

    for (auto const& [name, stream] : streams) {
        ImGui::SetCursorPosX((menuWidth / 2.0f) - (ImGui::CalcTextSize(name.c_str()).x / 2.0f));
        ImGui::Text("%s", name.c_str());

        ImGui::SetNextItemWidth(menuWidth);
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
            core::configManager.aquire();
            saveStreamConfig(name);
            core::configManager.release(true);
        }

        stream->sink->menuHandler();

        showVolumeSlider(name, "##_sdrpp_sink_menu_vol_", menuWidth);

        count++;
        if (count < maxCount) {
            ImGui::Spacing();
            ImGui::Separator();
        }
        ImGui::Spacing();
    }
}

std::vector<std::string> SinkManager::getStreamNames() {
    return streamNames;
}