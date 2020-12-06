#pragma once
#include <radio_demod.h>
#include <dsp/demodulator.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>
#include <dsp/audio.h>
#include <string>
#include <imgui.h>

class AMDemodulator : public Demodulator {
public:
    AMDemodulator() {}
    AMDemodulator(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth) {
        init(prefix, vfo, audioSampleRate, bandWidth);
    }

    void init(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth) {
        uiPrefix = prefix;
        _vfo = vfo;
        audioSampRate = audioSampleRate;
        bw = bandWidth;
        
        demod.init(_vfo->output);

        agc.init(&demod.out, 1.0f / 125.0f);

        float audioBW = std::min<float>(audioSampRate / 2.0f, bw / 2.0f);
        win.init(audioBW, audioBW, bbSampRate);
        resamp.init(&agc.out, &win, bbSampRate, audioSampRate);
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        resamp.updateWindow(&win);

        m2s.init(&resamp.out);
    }

    void start() {
        demod.start();
        agc.start();
        resamp.start();
        m2s.start();
        running = true;
    }

    void stop() {
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
        demod.setInput(_vfo->output);
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
        if (ImGui::InputFloat(("##_radio_am_bw_" + uiPrefix).c_str(), &bw, 1, 100, 0)) {
            bw = std::clamp<float>(bw, bwMin, bwMax);
            setBandwidth(bw);
        }

        ImGui::SetNextItemWidth(menuWidth - ImGui::CalcTextSize("Snap Interval").x - 8);
        ImGui::Text("Snap Interval");
        ImGui::SameLine();
        if (ImGui::InputFloat(("##_radio_am_snap_" + uiPrefix).c_str(), &snapInterval, 1, 100, 0)) {
            setSnapInterval(snapInterval);
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

    const float bwMax = 15000;
    const float bwMin = 6000;
    const float bbSampRate = 15000;

    std::string uiPrefix;
    float snapInterval = 1000;
    float audioSampRate = 48000;
    float bw = 12500;
    bool running = false;

    VFOManager::VFO* _vfo;
    dsp::AMDemod demod;
    dsp::AGC agc;
    dsp::filter_window::BlackmanWindow win;
    dsp::PolyphaseResampler<float> resamp;
    dsp::MonoToStereo m2s;

};