#pragma once
#include <radio_demod.h>
#include <dsp/demodulator.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>
#include <dsp/convertion.h>
#include <dsp/processing.h>
#include <dsp/math.h>
#include <dsp/audio.h>
#include <string>
#include <config.h>
#include <imgui.h>

class CWDemodulator : public Demodulator {
public:
    CWDemodulator() {}
    CWDemodulator(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth, ConfigManager* config) {
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
            if(!_config->conf[prefix].contains("CW")) {
                _config->conf[prefix]["CW"]["bandwidth"] = bw;
                _config->conf[prefix]["CW"]["snapInterval"] = snapInterval;
            }
            json conf = _config->conf[prefix]["CW"];
            bw = conf["bandwidth"];
            snapInterval = conf["snapInterval"];
        }
        else {
            _config->conf[prefix]["CW"]["bandwidth"] = bw;
            _config->conf[prefix]["CW"]["snapInterval"] = snapInterval;
        }
        _config->release(true);
        
        float audioBW = std::min<float>(audioSampRate / 2.0f, bw / 2.0f);
        win.init(audioBW, audioBW, bbSampRate);
        resamp.init(vfo->output, &win, bbSampRate, audioSampRate);
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        resamp.updateWindow(&win);

        xlator.init(&resamp.out, audioSampleRate, 1000);

        c2r.init(&xlator.out);

        agc.init(&c2r.out, 1.0f / 125.0f);

        m2s.init(&agc.out);
    }

    void start() {
        resamp.start();
        xlator.start();
        c2r.start();
        agc.start();
        m2s.start();
        running = true;
    }

    void stop() {
        resamp.stop();
        xlator.stop();
        c2r.stop();
        agc.stop();
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
        resamp.setInput(_vfo->output);
    }

    VFOManager::VFO* getVFO() {
        return _vfo;
    }
    
    void setAudioSampleRate(float sampleRate) {
        if (running) {
            resamp.stop();
            xlator.stop();
        }
        audioSampRate = sampleRate;
        float audioBW = std::min<float>(audioSampRate / 2.0f, bw / 2.0f);
        resamp.setOutSampleRate(audioSampRate);
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        win.setCutoff(audioBW);
        win.setTransWidth(audioBW);
        resamp.updateWindow(&win);
        xlator.setSampleRate(audioSampRate);
        if (running) {
            resamp.start();
            xlator.start();
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
        if (ImGui::InputFloat(("##_radio_cw_bw_" + uiPrefix).c_str(), &bw, 1, 100, 0)) {
            bw = std::clamp<float>(bw, bwMin, bwMax);
            setBandwidth(bw);
            _config->aquire();
            _config->conf[uiPrefix]["CW"]["bandwidth"] = bw;
            _config->release(true);
        }

        ImGui::Text("Snap Interval");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputFloat(("##_radio_cw_snap_" + uiPrefix).c_str(), &snapInterval, 1, 100, 0)) {
            setSnapInterval(snapInterval);
            _config->aquire();
            _config->conf[uiPrefix]["CW"]["snapInterval"] = snapInterval;
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

    const float bwMax = 500;
    const float bwMin = 100;
    const float bbSampRate = 500;

    std::string uiPrefix;
    float snapInterval = 10;
    float audioSampRate = 48000;
    float bw = 200;
    bool running = false;

    VFOManager::VFO* _vfo;
    dsp::filter_window::BlackmanWindow win;
    dsp::PolyphaseResampler<dsp::complex_t> resamp;
    dsp::FrequencyXlator<dsp::complex_t> xlator;
    dsp::ComplexToReal c2r;
    dsp::AGC agc;
    dsp::MonoToStereo m2s;

    ConfigManager* _config;

};