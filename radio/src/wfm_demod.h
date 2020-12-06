#pragma once
#include <radio_demod.h>
#include <dsp/demodulator.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>
#include <dsp/audio.h>
#include <string>
#include <imgui.h>

class WFMDemodulator : public Demodulator {
public:
    WFMDemodulator() {}
    WFMDemodulator(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth) {
        init(prefix, vfo, audioSampleRate, bandWidth);
    }

    void init(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth) {
        uiPrefix = prefix;
        _vfo = vfo;
        audioSampRate = audioSampleRate;
        bw = bandWidth;
        
        demod.init(_vfo->output, bbSampRate, bandWidth / 2.0f);

        float audioBW = std::min<float>(audioSampleRate / 2.0f, 16000.0f);
        win.init(audioBW, audioBW, bbSampRate);
        resamp.init(&demod.out, &win, bbSampRate, audioSampRate);
        win.setSampleRate(bbSampRate * resamp.getInterpolation());
        resamp.updateWindow(&win);

        deemp.init(&resamp.out, audioSampRate, tau);

        m2s.init(&deemp.out);
    }

    void start() {
        demod.start();
        resamp.start();
        deemp.start();
        m2s.start();
        running = true;
    }

    void stop() {
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
        demod.setInput(_vfo->output);
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
        if (ImGui::InputFloat(("##_radio_wfm_bw_" + uiPrefix).c_str(), &bw, 1, 100, 0)) {
            bw = std::clamp<float>(bw, bwMin, bwMax);
            setBandwidth(bw);
        }

        ImGui::SetNextItemWidth(menuWidth - ImGui::CalcTextSize("Snap Interval").x - 8);
        ImGui::Text("Snap Interval");
        ImGui::SameLine();
        if (ImGui::InputFloat(("##_radio_wfm_snap_" + uiPrefix).c_str(), &snapInterval, 1, 100, 0)) {
            setSnapInterval(snapInterval);
        }

        ImGui::SetNextItemWidth(menuWidth - ImGui::CalcTextSize("De-emphasis").x - 8);
        ImGui::Text("De-emphasis");
        ImGui::SameLine();
        if (ImGui::Combo(("##_radio_wfm_deemp_" + uiPrefix).c_str(), &deempId, deempModes)) {
            setDeempIndex(deempId);
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

    const float bwMax = 200000;
    const float bwMin = 100000;
    const float bbSampRate = 200000;
    const char* deempModes = "50µS\00075µS\000none\000";
    const float deempVals[2] = { 50e-6, 75e-6 };

    std::string uiPrefix;
    float snapInterval = 100000;
    float audioSampRate = 48000;
    float bw = 200000;
    int deempId = 0;
    float tau = 50e-6;
    bool running = false;

    VFOManager::VFO* _vfo;
    dsp::FMDemod demod;
    dsp::filter_window::BlackmanWindow win;
    dsp::PolyphaseResampler<float> resamp;
    dsp::BFMDeemp deemp;
    dsp::MonoToStereo m2s;

};