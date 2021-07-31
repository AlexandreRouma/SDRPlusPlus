#pragma once
#include <radio_demod.h>
#include <dsp/demodulator.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>
#include <dsp/audio.h>
#include <string>
#include <config.h>
#include <imgui.h>


class LSBDemodulator : public Demodulator {
public:
    LSBDemodulator() {}
    LSBDemodulator(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth, ConfigManager* config) {
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
            if(!_config->conf[prefix].contains("LSB")) {
                _config->conf[prefix]["LSB"]["bandwidth"] = bw;
                _config->conf[prefix]["LSB"]["snapInterval"] = snapInterval;
                _config->conf[prefix]["LSB"]["squelchLevel"] = squelchLevel;
            }
            json conf = _config->conf[prefix]["LSB"];
            if (conf.contains("bandwidth")) { bw = conf["bandwidth"]; }
            if (conf.contains("snapInterval")) { snapInterval = conf["snapInterval"]; }
            if (conf.contains("squelchLevel")) { squelchLevel = conf["squelchLevel"]; }
        }
        else {
            _config->conf[prefix]["LSB"]["bandwidth"] = bw;
            _config->conf[prefix]["LSB"]["snapInterval"] = snapInterval;
            _config->conf[prefix]["LSB"]["squelchLevel"] = squelchLevel;
        }
        _config->release(true);

        squelch.init(_vfo->output, squelchLevel);
        
        demod.init(&squelch.out, bbSampRate, bw, dsp::SSBDemod::MODE_LSB);

        agc.init(&demod.out, 20.0f, bbSampRate);

        float audioBW = std::min<float>(audioSampRate / 2.0f, bw);
        win.init(audioBW, audioBW, bbSampRate);
        resamp.init(&agc.out, &win, bbSampRate, audioSampRate);
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        resamp.updateWindow(&win);

        m2s.init(&resamp.out);
    }

    void start() {
        squelch.start();
        demod.start();
        agc.start();
        resamp.start();
        m2s.start();
        running = true;
    }

    void stop() {
        squelch.stop();
        demod.stop();
        agc.stop();
        resamp.stop();
        m2s.stop();
        running = false;
    }
    
    bool isRunning() {
        return running;
    }

    void select() {
        _vfo->setSampleRate(bbSampRate, bw);
        _vfo->setSnapInterval(snapInterval);
        _vfo->setReference(ImGui::WaterfallVFO::REF_UPPER);
        _vfo->setBandwidthLimits(bwMin, bwMax, false);
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
        }
        audioSampRate = sampleRate;
        float audioBW = std::min<float>(audioSampRate / 2.0f, bw);
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
        return &m2s.out;
    }
    
    void showMenu() {
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::InputFloat(("##_radio_lsb_bw_" + uiPrefix).c_str(), &bw, 1, 100, "%.0f", 0)) {
            bw = std::clamp<float>(bw, bwMin, bwMax);
            setBandwidth(bw);
            _config->acquire();
            _config->conf[uiPrefix]["LSB"]["bandwidth"] = bw;
            _config->release(true);
        }if (running) {
            if (_vfo->getBandwidthChanged()) {
                bw = _vfo->getBandwidth();
                setBandwidth(bw, false);
                _config->acquire();
                _config->conf[uiPrefix]["LSB"]["bandwidth"] = bw;
                _config->release(true);
            }
        }

        ImGui::Text("Snap Interval");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputFloat(("##_radio_lsb_snap_" + uiPrefix).c_str(), &snapInterval, 1, 100, "%.0f", 0)) {
            if (snapInterval < 1) { snapInterval = 1; }
            setSnapInterval(snapInterval);
            _config->acquire();
            _config->conf[uiPrefix]["LSB"]["snapInterval"] = snapInterval;
            _config->release(true);
        }

        ImGui::Text("Squelch");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderFloat(("##_radio_lsb_squelch_" + uiPrefix).c_str(), &squelchLevel, -100.0f, 0.0f, "%.3fdB")) {
            squelch.setLevel(squelchLevel);
            _config->acquire();
            _config->conf[uiPrefix]["LSB"]["squelchLevel"] = squelchLevel;
            _config->release(true);
        }
    } 

    void setBandwidth(float bandWidth, bool updateWaterfall = true) {
        bandWidth = std::clamp<float>(bandWidth, bwMin, bwMax);
        bw = bandWidth;
        _vfo->setBandwidth(bw, updateWaterfall);
        demod.setBandWidth(bw);
        float audioBW = std::min<float>(audioSampRate / 2.0f, bw);
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        win.setCutoff(audioBW);
        win.setTransWidth(audioBW);
        resamp.updateWindow(&win);
    }

    void saveParameters(bool lock = true) {
        if (lock) { _config->acquire(); }
        _config->conf[uiPrefix]["WFM"]["bandwidth"] = bw;
        _config->conf[uiPrefix]["WFM"]["snapInterval"] = snapInterval;
        _config->conf[uiPrefix]["WFM"]["squelchLevel"] = squelchLevel;
        if (lock) { _config->release(true); }
    }

private:
    void setSnapInterval(float snapInt) {
        snapInterval = snapInt;
        _vfo->setSnapInterval(snapInterval);
    }

    const float bwMax = 12000;
    const float bwMin = 500;
    const float bbSampRate = 24000;

    std::string uiPrefix;
    float snapInterval = 100;
    float audioSampRate = 48000;
    float bw = 3000;
    bool running = false;
    float squelchLevel = -100.0f;

    VFOManager::VFO* _vfo;
    dsp::Squelch squelch;
    dsp::SSBDemod demod;
    dsp::AGC agc;
    dsp::filter_window::BlackmanWindow win;
    dsp::PolyphaseResampler<float> resamp;
    dsp::MonoToStereo m2s;

    ConfigManager* _config;

};