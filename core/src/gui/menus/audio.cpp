#include <gui/menus/audio.h>
#include <gui/bandplan.h>
#include <gui/gui.h>
#include <core.h>
#include <signal_path/audio.h>

namespace audiomenu {
    // Note: Those are supposed to be the ones from the volume slider
    std::string audioStreamName;
    float* volume;

    void loadAudioConfig(std::string name) {
        json audioSettings = core::configManager.conf["audio"][name];
        std::string devName = audioSettings["device"];
        auto _devIt = std::find(audio::streams[name]->audio->deviceNames.begin(), audio::streams[name]->audio->deviceNames.end(), devName);

        // If audio device doesn't exist anymore
        if (_devIt == audio::streams[name]->audio->deviceNames.end()) {
            audio::streams[name]->audio->setToDefault();
            int deviceId = audio::streams[name]->audio->getDeviceId();
            audio::setAudioDevice(name, deviceId, audio::streams[name]->audio->devices[deviceId].sampleRates[0]);
            audio::streams[name]->sampleRateId = 0;
            audio::streams[name]->volume = audioSettings["volume"];
            audio::streams[name]->audio->setVolume(audio::streams[name]->volume);
            return;
        }
        int deviceId = std::distance(audio::streams[name]->audio->deviceNames.begin(), _devIt);
        float sr = audioSettings["sampleRate"];
        auto _srIt = std::find(audio::streams[name]->audio->devices[deviceId].sampleRates.begin(), audio::streams[name]->audio->devices[deviceId].sampleRates.end(), sr);

        // If sample rate doesn't exist anymore
        if (_srIt == audio::streams[name]->audio->devices[deviceId].sampleRates.end()) {
            audio::streams[name]->sampleRateId = 0;
            audio::setAudioDevice(name, deviceId, audio::streams[name]->audio->devices[deviceId].sampleRates[0]);
            audio::streams[name]->volume = audioSettings["volume"];
            audio::streams[name]->audio->setVolume(audio::streams[name]->volume);
            return;
        }

        int samplerateId = std::distance(audio::streams[name]->audio->devices[deviceId].sampleRates.begin(), _srIt);
        audio::streams[name]->sampleRateId = samplerateId;
        audio::setAudioDevice(name, deviceId, audio::streams[name]->audio->devices[deviceId].sampleRates[samplerateId]);
        audio::streams[name]->deviceId = deviceId;
        audio::streams[name]->volume = audioSettings["volume"];
        audio::streams[name]->audio->setVolume(audio::streams[name]->volume);
    }

    void saveAudioConfig(std::string name) {
        core::configManager.conf["audio"][name]["device"] = audio::streams[name]->audio->deviceNames[audio::streams[name]->deviceId];
        core::configManager.conf["audio"][name]["volume"] = audio::streams[name]->volume;
        core::configManager.conf["audio"][name]["sampleRate"] = audio::streams[name]->sampleRate;
    }

    void init() {
        for (auto [name, stream] : audio::streams) {
            if (core::configManager.conf["audio"].contains(name)) {
                bool running = audio::streams[name]->running;
                audio::stopStream(name);
                loadAudioConfig(name);
                if (running) {
                    audio::startStream(name);
                }
            }
        }

        audioStreamName = audio::getNameFromVFO(gui::waterfall.selectedVFO);
        if (audioStreamName != "") {
            volume = &audio::streams[audioStreamName]->volume;
        }
    }

    void draw(void* ctx) {
        float menuColumnWidth = ImGui::GetContentRegionAvailWidth();
        int count = 0;
        int maxCount = audio::streams.size();
        for (auto const& [name, stream] : audio::streams) {
            int deviceId;
            float vol = 1.0f;
            deviceId = stream->audio->getDeviceId();

            ImGui::SetCursorPosX((menuColumnWidth / 2.0f) - (ImGui::CalcTextSize(name.c_str()).x / 2.0f));
            ImGui::Text("%s", name.c_str());

            ImGui::PushItemWidth(menuColumnWidth);
            bool running = stream->running;
            if (ImGui::Combo(("##_audio_dev_0_"+ name).c_str(), &stream->deviceId, stream->audio->devTxtList.c_str())) {
                audio::stopStream(name);
                audio::setAudioDevice(name, stream->deviceId, stream->audio->devices[deviceId].sampleRates[0]);
                if (running) {
                    audio::startStream(name);
                }
                stream->sampleRateId = 0;

                // Create config if it doesn't exist
                core::configManager.aquire();
                if (!core::configManager.conf["audio"].contains(name)) {
                    saveAudioConfig(name);
                }
                core::configManager.conf["audio"][name]["device"] = stream->audio->deviceNames[stream->deviceId];
                core::configManager.conf["audio"][name]["sampleRate"] = stream->audio->devices[stream->deviceId].sampleRates[0];
                core::configManager.release(true);
            }
            if (ImGui::Combo(("##_audio_sr_0_" + name).c_str(), &stream->sampleRateId, stream->audio->devices[deviceId].txtSampleRates.c_str())) {
                audio::stopStream(name);
                audio::setSampleRate(name, stream->audio->devices[deviceId].sampleRates[stream->sampleRateId]);
                if (running) {
                    audio::startStream(name);
                }

                // Create config if it doesn't exist
                core::configManager.aquire();
                if (!core::configManager.conf["audio"].contains(name)) {
                    saveAudioConfig(name);
                }
                core::configManager.conf["audio"][name]["sampleRate"] = stream->audio->devices[deviceId].sampleRates[stream->sampleRateId];
                core::configManager.release(true);
            }
            if (ImGui::SliderFloat(("##_audio_vol_0_" + name).c_str(), &stream->volume, 0.0f, 1.0f, "")) {
                stream->audio->setVolume(stream->volume);

                // Create config if it doesn't exist
                core::configManager.aquire();
                if (!core::configManager.conf["audio"].contains(name)) {
                    saveAudioConfig(name);
                }
                core::configManager.conf["audio"][name]["volume"] = stream->volume;
                core::configManager.release(true);
            }
            ImGui::PopItemWidth();
            count++;
            if (count < maxCount) {
                ImGui::Spacing();
                ImGui::Separator();
            }
            ImGui::Spacing();
        }
    }
};