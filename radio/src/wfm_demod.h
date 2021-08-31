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

        _config->acquire();
        if(_config->conf.contains(prefix)) {
            if(!_config->conf[prefix].contains("WFM")) {
                _config->conf[prefix]["WFM"]["bandwidth"] = bw;
                _config->conf[prefix]["WFM"]["snapInterval"] = snapInterval;
                _config->conf[prefix]["WFM"]["deempMode"] = deempId;
                _config->conf[prefix]["WFM"]["squelchLevel"] = squelchLevel;
                _config->conf[prefix]["WFM"]["stereo"] = false;
            }

            // Correct for new settings
            if (!config->conf[prefix]["WFM"].contains("stereo")) { _config->conf[prefix]["WFM"]["stereo"] = false;}

            json conf = _config->conf[prefix]["WFM"];
            bw = conf["bandwidth"];
            snapInterval = conf["snapInterval"];
            deempId = conf["deempMode"];
            squelchLevel = conf["squelchLevel"];
            stereo = conf["stereo"];
        }
        else {
            _config->conf[prefix]["WFM"]["bandwidth"] = bw;
            _config->conf[prefix]["WFM"]["snapInterval"] = snapInterval;
            _config->conf[prefix]["WFM"]["deempMode"] = deempId; 
            _config->conf[prefix]["WFM"]["squelchLevel"] = squelchLevel;
            _config->conf[prefix]["WFM"]["stereo"] = false;
        }
        _config->release(true);
        
        squelch.init(_vfo->output, squelchLevel);
        
        demod.init(&squelch.out, bbSampRate, bw / 2.0f);
        demodStereo.init(&squelch.out, bbSampRate, bw / 2.0f);

        float audioBW = std::min<float>(audioSampleRate / 2.0f, 16000.0f);
        win.init(audioBW, audioBW, bbSampRate);
        if (stereo) {
            resamp.init(demodStereo.out, &win, bbSampRate, audioSampRate);
        }
        else {
            resamp.init(&demod.out, &win, bbSampRate, audioSampRate);
        }
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        resamp.updateWindow(&win);

        deemp.init(&resamp.out, audioSampRate, tau);

        if (deempId == 2) { deemp.bypass = true; }

        onUserChangedBandwidthHandler.handler = vfoUserChangedBandwidthHandler;
        onUserChangedBandwidthHandler.ctx = this;

        _vfo->wtfVFO->onUserChangedBandwidth.bindHandler(&onUserChangedBandwidthHandler);
    }

    void start() {
        squelch.start();
        if (stereo) {
            demodStereo.start();
        }
        else {
            demod.start();
        }
        resamp.start();
        deemp.start();
        running = true;
    }

    void stop() {
        squelch.stop();
        if (stereo) {
            demodStereo.stop();
        }
        else {
            demod.stop();
        }
        resamp.stop();
        deemp.stop();
        running = false;
    }
    
    bool isRunning() {
        return running;
    }

    void select() {
        _vfo->setSampleRate(bbSampRate, bw);
        _vfo->setSnapInterval(snapInterval);
        _vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        _vfo->setBandwidthLimits(bwMin, bwMax, false);
    }

    void setVFO(VFOManager::VFO* vfo) {
        _vfo = vfo;
        squelch.setInput(_vfo->output);
        _vfo->wtfVFO->onUserChangedBandwidth.bindHandler(&onUserChangedBandwidthHandler);
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
        return &deemp.out;
    }
    
    void showMenu() {
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::InputFloat(("##_radio_wfm_bw_" + uiPrefix).c_str(), &bw, 1, 100, "%.0f", 0)) {
            bw = std::clamp<float>(bw, bwMin, bwMax);
            setBandwidth(bw);
            _config->acquire();
            _config->conf[uiPrefix]["WFM"]["bandwidth"] = bw;
            _config->release(true);
        }

        ImGui::LeftLabel("Snap Interval");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputFloat(("##_radio_wfm_snap_" + uiPrefix).c_str(), &snapInterval, 1, 100, "%.0f", 0)) {
            if (snapInterval < 1) { snapInterval = 1; }
            setSnapInterval(snapInterval);
            _config->acquire();
            _config->conf[uiPrefix]["WFM"]["snapInterval"] = snapInterval;
            _config->release(true);
        }

        
        ImGui::LeftLabel("De-emphasis");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(("##_radio_wfm_deemp_" + uiPrefix).c_str(), &deempId, deempModes)) {
            setDeempIndex(deempId);
            _config->acquire();
            _config->conf[uiPrefix]["WFM"]["deempMode"] = deempId;
            _config->release(true);
        }

        ImGui::LeftLabel("Squelch");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderFloat(("##_radio_wfm_sqelch_" + uiPrefix).c_str(), &squelchLevel, -100.0f, 0.0f, "%.3fdB")) {
            squelch.setLevel(squelchLevel);
            _config->acquire();
            _config->conf[uiPrefix]["WFM"]["squelchLevel"] = squelchLevel;
            _config->release(true);
        }

        if (ImGui::Checkbox(("Stereo##_radio_wfm_stereo_" + uiPrefix).c_str(), &stereo)) {
            setStereo(stereo);
            _config->acquire();
            _config->conf[uiPrefix]["WFM"]["stereo"] = stereo;
            _config->release(true);
        }
    }

    static void vfoUserChangedBandwidthHandler(double newBw, void* ctx) {
        WFMDemodulator* _this = (WFMDemodulator*)ctx;
        if (_this->running) {
            _this->bw = newBw;
            _this->setBandwidth(_this->bw, false);
            _this->_config->acquire();
            _this->_config->conf[_this->uiPrefix]["WFM"]["bandwidth"] = _this->bw;
            _this->_config->release(true);
        }
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

    void setBandwidth(float bandWidth, bool updateWaterfall = true) {
        bandWidth = std::clamp<float>(bandWidth, bwMin, bwMax);
        bw = bandWidth;
        _vfo->setBandwidth(bw, updateWaterfall);
        demod.setDeviation(bw / 2.0f);
        demodStereo.setDeviation(bw / 2.0f);
    }

    void setStereo(bool enabled) {
        if (running) {
            demod.stop();
            demodStereo.stop();
        }

        if (enabled) {
            resamp.setInput(demodStereo.out);
            if (running) { demodStereo.start(); }
        }
        else {
            resamp.setInput(&demod.out);
            if (running) { demod.start(); }
        }
    }

    void saveParameters(bool lock = true) {
        if (lock) { _config->acquire(); }
        _config->conf[uiPrefix]["WFM"]["bandwidth"] = bw;
        _config->conf[uiPrefix]["WFM"]["snapInterval"] = snapInterval;
        _config->conf[uiPrefix]["WFM"]["deempMode"] = deempId; 
        _config->conf[uiPrefix]["WFM"]["squelchLevel"] = squelchLevel;
        _config->conf[uiPrefix]["WFM"]["stereo"] = stereo;
        if (lock) { _config->release(true); }
    }

private:

    const float bwMax = 250000;
    const float bwMin = 50000;
    const float bbSampRate = 250000;
    const char* deempModes = "50µs\00075µs\000none\000";
    const float deempVals[2] = { 50e-6, 75e-6 };

    std::string uiPrefix;
    float snapInterval = 100000;
    float audioSampRate = 48000;
    float squelchLevel = -100.0f;
    float bw = 200000;
    bool stereo = false;
    int deempId = 0;
    float tau = 50e-6;
    bool running = false;

    VFOManager::VFO* _vfo;
    dsp::Squelch squelch;

    dsp::FMDemod demod;
    dsp::StereoFMDemod demodStereo;

    dsp::filter_window::BlackmanWindow win;
    dsp::PolyphaseResampler<dsp::stereo_t> resamp;
    dsp::BFMDeemp deemp;

    ConfigManager* _config;

    EventHandler<double> onUserChangedBandwidthHandler;

};