#pragma once
#include <radio_demod.h>
#include <dsp/demodulator.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>
#include <dsp/audio.h>
#include <string>
#include <config.h>
#include <imgui.h>


class DSBDemodulator : public Demodulator {
public:
    DSBDemodulator() {}
    DSBDemodulator(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth, ConfigManager* config) {
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
            if(!_config->conf[prefix].contains("DSB")) {
                if (!_config->conf[prefix]["DSB"].contains("bandwidth")) { _config->conf[prefix]["DSB"]["bandwidth"] = bw; }
                if (!_config->conf[prefix]["DSB"].contains("snapInterval")) { _config->conf[prefix]["DSB"]["snapInterval"] = snapInterval; }
            }
            json conf = _config->conf[prefix]["DSB"];
            bw = conf["bandwidth"];
            snapInterval = conf["snapInterval"];
        }
        else {
            _config->conf[prefix]["DSB"]["bandwidth"] = bw;
            _config->conf[prefix]["DSB"]["snapInterval"] = snapInterval;
        }
        _config->release(true);

        squelch.init(_vfo->output, squelchLevel);
        
        demod.init(&squelch.out, bbSampRate, bandWidth, dsp::SSBDemod::MODE_DSB);

        agc.init(&demod.out, 20.0f, bbSampRate);

        float audioBW = std::min<float>(audioSampRate / 2.0f, bw / 2.0f);
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
        return &m2s.out;
    }
    
    void showMenu() {
        float menuWidth = ImGui::GetContentRegionAvailWidth();

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::InputFloat(("##_radio_dsb_bw_" + uiPrefix).c_str(), &bw, 1, 100, "%.0f", 0)) {
            bw = std::clamp<float>(bw, bwMin, bwMax);
            setBandwidth(bw);
            _config->aquire();
            _config->conf[uiPrefix]["DSB"]["bandwidth"] = bw;
            _config->release(true);
        }

        ImGui::Text("Snap Interval");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputFloat(("##_radio_dsb_snap_" + uiPrefix).c_str(), &snapInterval, 1, 100, "%.0f", 0)) {
            setSnapInterval(snapInterval);
            _config->aquire();
            _config->conf[uiPrefix]["DSB"]["snapInterval"] = snapInterval;
            _config->release(true);
        }

        ImGui::Text("Squelch");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderFloat(("##_radio_dsb_squelch_" + uiPrefix).c_str(), &squelchLevel, -100.0f, 0.0f, "%.3fdB")) {
            squelch.setLevel(squelchLevel);
            _config->aquire();
            _config->conf[uiPrefix]["DSB"]["squelchLevel"] = squelchLevel;
            _config->release(true);
        }
    } 

private:
    void setBandwidth(float bandWidth) {
        bw = bandWidth;
        _vfo->setBandwidth(bw);
    }

    void setSnapInterval(float snapInt) {
        snapInterval = snapInt;
        _vfo->setSnapInterval(snapInterval);
    }

    const float bwMax = 12000;
    const float bwMin = 1000;
    const float bbSampRate = 12000;

    std::string uiPrefix;
    float snapInterval = 100;
    float audioSampRate = 48000;
    float bw = 6000;
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