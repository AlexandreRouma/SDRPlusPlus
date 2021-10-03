#pragma once
#include <radio_demod.h>
#include <dsp/demodulator.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>
#include <dsp/audio.h>
#include <string>
#include <config.h>
#include <imgui.h>


class FMDemodulator : public Demodulator {
public:
    FMDemodulator() {}
    FMDemodulator(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth, ConfigManager* config) {
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
            if(!_config->conf[prefix].contains("FM")) {
                _config->conf[prefix]["FM"]["bandwidth"] = bw;
                _config->conf[prefix]["FM"]["snapInterval"] = snapInterval;
                _config->conf[prefix]["FM"]["squelchLevel"] = squelchLevel;
            }
            json conf = _config->conf[prefix]["FM"];
            if (conf.contains("bandwidth")) { bw = conf["bandwidth"]; }
            if (conf.contains("snapInterval")) { snapInterval = conf["snapInterval"]; }
            if (conf.contains("squelchLevel")) { squelchLevel = conf["squelchLevel"]; }
        }
        else {
            _config->conf[prefix]["FM"]["bandwidth"] = bw;
            _config->conf[prefix]["FM"]["snapInterval"] = snapInterval;
            _config->conf[prefix]["FM"]["squelchLevel"] = squelchLevel;
        }
        _config->release(true);

        squelch.init(_vfo->output, squelchLevel);
        
        demod.init(&squelch.out, bbSampRate, bw / 2.0f);

        float audioBW = std::min<float>(audioSampleRate / 2.0f, bw / 2.0f);
        win.init(audioBW, audioBW, bbSampRate);
        resamp.init(&demod.out, &win, bbSampRate, audioSampRate);
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        resamp.updateWindow(&win);

        onUserChangedBandwidthHandler.handler = vfoUserChangedBandwidthHandler;
        onUserChangedBandwidthHandler.ctx = this;

        _vfo->wtfVFO->onUserChangedBandwidth.bindHandler(&onUserChangedBandwidthHandler);
    }

    void start() {
        squelch.start();
        demod.start();
        resamp.start();
        running = true;
    }

    void stop() {
        squelch.stop();
        demod.stop();
        resamp.stop();
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
        }
        audioSampRate = sampleRate;
        float audioBW = std::min<float>(audioSampRate / 2.0f, bw / 2.0f);
        resamp.setOutSampleRate(audioSampRate);
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        win.setCutoff(audioBW);
        win.setTransWidth(audioBW);
        resamp.updateWindow(&win);
        if (running) {
            resamp.start();
        }
    }
    
    float getAudioSampleRate() {
        return audioSampRate;
    }

    dsp::stream<dsp::stereo_t>* getOutput() {
        return &resamp.out;
    }
    
    void showMenu() {
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::InputFloat(("##_radio_fm_bw_" + uiPrefix).c_str(), &bw, 1, 100, "%.0f", 0)) {
            bw = std::clamp<float>(bw, bwMin, bwMax);
            setBandwidth(bw);
            _config->acquire();
            _config->conf[uiPrefix]["FM"]["bandwidth"] = bw;
            _config->release(true);
        }
        
        ImGui::LeftLabel("Snap Interval");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputFloat(("##_radio_fm_snap_" + uiPrefix).c_str(), &snapInterval, 1, 100, "%.0f", 0)) {
            if (snapInterval < 1) { snapInterval = 1; }
            setSnapInterval(snapInterval);
            _config->acquire();
            _config->conf[uiPrefix]["FM"]["snapInterval"] = snapInterval;
            _config->release(true);
        }

        ImGui::LeftLabel("Squelch");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderFloat(("##_radio_fm_squelch_" + uiPrefix).c_str(), &squelchLevel, -100.0f, 0.0f, "%.3fdB")) {
            squelch.setLevel(squelchLevel);
            _config->acquire();
            _config->conf[uiPrefix]["FM"]["squelchLevel"] = squelchLevel;
            _config->release(true);
        }
    }

    static void vfoUserChangedBandwidthHandler(double newBw, void* ctx) {
        FMDemodulator* _this = (FMDemodulator*)ctx;
        if (_this->running) {
            _this->bw = newBw;
            _this->setBandwidth(_this->bw, false);
            _this->_config->acquire();
            _this->_config->conf[_this->uiPrefix]["FM"]["bandwidth"] = _this->bw;
            _this->_config->release(true);
        }
    }

    void setBandwidth(float bandWidth, bool updateWaterfall = true) {
        bandWidth = std::clamp<float>(bandWidth, bwMin, bwMax);
        bw = bandWidth;
        _vfo->setBandwidth(bw, updateWaterfall);
        demod.setDeviation(bw / 2.0f);
        setAudioSampleRate(audioSampRate);
    }

    void saveParameters(bool lock = true) {
        if (lock) { _config->acquire(); }
        _config->conf[uiPrefix]["FM"]["bandwidth"] = bw;
        _config->conf[uiPrefix]["FM"]["snapInterval"] = snapInterval;
        _config->conf[uiPrefix]["FM"]["squelchLevel"] = squelchLevel;
        if (lock) { _config->release(true); }
    }

private:
    void setSnapInterval(float snapInt) {
        snapInterval = snapInt;
        _vfo->setSnapInterval(snapInterval);
    }

    const float bwMax = 50000;
    const float bwMin = 1000;
    const float bbSampRate = 50000;

    std::string uiPrefix;
    float snapInterval = 2500;
    float audioSampRate = 48000;
    float bw = 50000;
    bool running = false;
    float squelchLevel = -100.0f;

    VFOManager::VFO* _vfo;
    dsp::Squelch squelch;
    dsp::FMDemod demod;
    dsp::filter_window::BlackmanWindow win;
    dsp::PolyphaseResampler<dsp::stereo_t> resamp;

    ConfigManager* _config;

    EventHandler<double> onUserChangedBandwidthHandler;

};