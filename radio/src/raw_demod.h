#pragma once
#include <radio_demod.h>
#include <dsp/convertion.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>
#include <dsp/audio.h>
#include <string>
#include <config.h>
#include <imgui.h>


class RAWDemodulator : public Demodulator {
public:
    RAWDemodulator() {}
    RAWDemodulator(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth, ConfigManager* config) {
        init(prefix, vfo, audioSampleRate, bandWidth, config);
    }

    void init(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth, ConfigManager* config) {
        uiPrefix = prefix;
        _vfo = vfo;
        audioSampRate = audioSampleRate;
        bw = bandWidth;
        _config = config;

        _config->aquire();
        if(_config->conf.contains(prefix)) {
            if(!_config->conf[prefix].contains("RAW")) {
               if (!_config->conf[prefix]["RAW"].contains("snapInterval")) { _config->conf[prefix]["RAW"]["snapInterval"] = snapInterval; }
            }
            json conf = _config->conf[prefix]["RAW"];
            snapInterval = conf["snapInterval"];
        }
        else {
            _config->conf[prefix]["RAW"]["snapInterval"] = snapInterval;
        }
        _config->release(true);

        squelch.init(_vfo->output, squelchLevel);
        
        c2s.init(&squelch.out);
    }

    void start() {
        squelch.start();
        c2s.start();
        running = true;
    }

    void stop() {
        squelch.stop();
        c2s.stop();
        running = false;
    }
    
    bool isRunning() {
        return running;
    }

    void select() {
        _vfo->setSampleRate(audioSampRate, audioSampRate);
        _vfo->setSnapInterval(snapInterval);
        _vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        _vfo->setBandwidthLimits(0, 0, true);
    }

    void setVFO(VFOManager::VFO* vfo) {
        _vfo = vfo;
        squelch.setInput(_vfo->output);
    }

    VFOManager::VFO* getVFO() {
        return _vfo;
    }
    
    void setAudioSampleRate(float sampleRate) {
        audioSampRate = sampleRate;
        if (running) {
            _vfo->setSampleRate(audioSampRate, audioSampRate);
        }
    }
    
    float getAudioSampleRate() {
        return audioSampRate;
    }

    dsp::stream<dsp::stereo_t>* getOutput() {
        return &c2s.out;
    }
    
    void showMenu() {
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        ImGui::Text("Snap Interval");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputFloat(("##_radio_raw_snap_" + uiPrefix).c_str(), &snapInterval, 1, 100, "%.0f", 0)) {
            if (snapInterval < 1) { snapInterval = 1; }
            setSnapInterval(snapInterval);
            _config->aquire();
            _config->conf[uiPrefix]["RAW"]["snapInterval"] = snapInterval;
            _config->release(true);
        }

        ImGui::Text("Squelch");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderFloat(("##_radio_raw_squelch_" + uiPrefix).c_str(), &squelchLevel, -100.0f, 0.0f, "%.3fdB")) {
            squelch.setLevel(squelchLevel);
            _config->aquire();
            _config->conf[uiPrefix]["RAW"]["squelchLevel"] = squelchLevel;
            _config->release(true);
        }

        // TODO: Allow selection of the bandwidth
    } 

private:
    void setSnapInterval(float snapInt) {
        snapInterval = snapInt;
        _vfo->setSnapInterval(snapInterval);
    }

    std::string uiPrefix;
    float snapInterval = 10000;
    float audioSampRate = 48000;
    float bw = 12500;
    bool running = false;
    float squelchLevel = -100.0f;

    VFOManager::VFO* _vfo;
    dsp::Squelch squelch;
    dsp::ComplexToStereo c2s;

    ConfigManager* _config;

};