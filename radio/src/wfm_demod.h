#pragma once
#include <radio_demod.h>
#include <dsp/demodulator.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>
#include <dsp/audio.h>
#include <string>
#include <config.h>
#include <imgui.h>


class WFMDemodulator : public Demodulator {
public:
    WFMDemodulator() {}
    WFMDemodulator(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth, ConfigManager* config) {
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
            if(!_config->conf[prefix].contains("WFM")) {
                if (!_config->conf[prefix]["WFM"].contains("bandwidth")) { _config->conf[prefix]["WFM"]["bandwidth"] = bw; }
                if (!_config->conf[prefix]["WFM"].contains("snapInterval")) { _config->conf[prefix]["WFM"]["snapInterval"] = snapInterval; }
                if (!_config->conf[prefix]["WFM"].contains("deempMode")) { _config->conf[prefix]["WFM"]["deempMode"] = deempId; }
                if (!_config->conf[prefix]["WFM"].contains("squelchLevel")) { _config->conf[prefix]["WFM"]["squelchLevel"] = squelchLevel; }
            }
            json conf = _config->conf[prefix]["WFM"];
            bw = conf["bandwidth"];
            snapInterval = conf["snapInterval"];
            deempId = conf["deempMode"];
            squelchLevel = conf["squelchLevel"];
        }
        else {
            _config->conf[prefix]["WFM"]["bandwidth"] = bw;
            _config->conf[prefix]["WFM"]["snapInterval"] = snapInterval;
            _config->conf[prefix]["WFM"]["deempMode"] = deempId; 
            _config->conf[prefix]["WFM"]["squelchLevel"] = squelchLevel;
        }
        _config->release(true);
        
        squelch.init(_vfo->output, squelchLevel);
        
        demod.init(&squelch.out, bbSampRate, bw / 2.0f);

        float audioBW = std::min<float>(audioSampleRate / 2.0f, 16000.0f);
        win.init(audioBW, audioBW, bbSampRate);
        resamp.init(&demod.out, &win, bbSampRate, audioSampRate);
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        resamp.updateWindow(&win);

        deemp.init(&resamp.out, audioSampRate, tau);

        m2s.init(&deemp.out);
    }

    void start() {
        squelch.start();
        demod.start();
        resamp.start();
        deemp.start();
        m2s.start();
        running = true;
    }

    void stop() {
        squelch.stop();
        demod.stop();
        resamp.stop();
        deemp.stop();
        m2s.stop();
        running = false;
    }
    
    bool isRunning() {
        return running;
    }

    void select() {
        _vfo->setSampleRate(bbSampRate, bw);
        _vfo->setSnapInterval(snapInterval);
        _vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
    }

    void setVFO(VFOManager::VFO* vfo) {
        _vfo = vfo;
        squelch.setInput(_vfo->output);
    }

    VFOManager::VFO* getVFO() {
        return _vfo;
    }
    
    void setAudioSampleRate(float sampleRate) {
        if (running) {
            resamp.stop();
            deemp.stop();
        }
        audioSampRate = sampleRate;
        float audioBW = std::min<float>(audioSampRate / 2.0f, 16000.0f);
        resamp.setOutSampleRate(audioSampRate);
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        win.setCutoff(audioBW);
        win.setTransWidth(audioBW);
        resamp.updateWindow(&win);
        deemp.setSampleRate(audioSampRate);
        if (running) {
            resamp.start();
            deemp.start();
        }
    }
    
    float getAudioSampleRate() {
        return audioSampRate;
    }

    dsp::stream<dsp::stereo_t>* getOutput() {
        return &m2s.out;
    }
    
    void showMenu() {
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::InputFloat(("##_radio_wfm_bw_" + uiPrefix).c_str(), &bw, 1, 100, "%.0f", 0)) {
            bw = std::clamp<float>(bw, bwMin, bwMax);
            setBandwidth(bw);
            _config->aquire();
            _config->conf[uiPrefix]["WFM"]["bandwidth"] = bw;
            _config->release(true);
        }

        ImGui::Text("Snap Interval");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputFloat(("##_radio_wfm_snap_" + uiPrefix).c_str(), &snapInterval, 1, 100, "%.0f", 0)) {
            if (snapInterval < 1) { snapInterval = 1; }
            setSnapInterval(snapInterval);
            _config->aquire();
            _config->conf[uiPrefix]["WFM"]["snapInterval"] = snapInterval;
            _config->release(true);
        }

        
        ImGui::Text("De-emphasis");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(("##_radio_wfm_deemp_" + uiPrefix).c_str(), &deempId, deempModes)) {
            setDeempIndex(deempId);
            _config->aquire();
            _config->conf[uiPrefix]["WFM"]["deempMode"] = deempId;
            _config->release(true);
        }

        ImGui::Text("Squelch");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderFloat(("##_radio_wfm_sqelch_" + uiPrefix).c_str(), &squelchLevel, -100.0f, 0.0f, "%.3fdB")) {
            squelch.setLevel(squelchLevel);
            _config->aquire();
            _config->conf[uiPrefix]["WFM"]["squelchLevel"] = squelchLevel;
            _config->release(true);
        }
    } 

private:
    void setBandwidth(float bandWidth) {
        bw = bandWidth;
        _vfo->setBandwidth(bw);
        demod.setDeviation(bw / 2.0f);
    }

    void setDeempIndex(int id) {
        if (id >= 2 || id < 0) {
            deemp.bypass = true;
            return;
        }
        deemp.setTau(deempVals[id]);
        deemp.bypass = false;
    }

    void setSnapInterval(float snapInt) {
        snapInterval = snapInt;
        _vfo->setSnapInterval(snapInterval);
    }

    const float bwMax = 250000;
    const float bwMin = 50000;
    const float bbSampRate = 250000;
    const char* deempModes = "50µS\00075µS\000none\000";
    const float deempVals[2] = { 50e-6, 75e-6 };

    std::string uiPrefix;
    float snapInterval = 100000;
    float audioSampRate = 48000;
    float squelchLevel = -100.0f;
    float bw = 200000;
    int deempId = 0;
    float tau = 50e-6;
    bool running = false;

    VFOManager::VFO* _vfo;
    dsp::Squelch squelch;
    dsp::FMDemod demod;
    dsp::filter_window::BlackmanWindow win;
    dsp::PolyphaseResampler<float> resamp;
    dsp::BFMDeemp deemp;
    dsp::MonoToStereo m2s;

    ConfigManager* _config;

};