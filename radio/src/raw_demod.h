#pragma once
#include <radio_demod.h>
#include <dsp/convertion.h>
#include <dsp/resampling.h>
#include <dsp/filter.h>
#include <dsp/audio.h>
#include <string>
#include <imgui.h>

class RAWDemodulator : public Demodulator {
public:
    RAWDemodulator() {}
    RAWDemodulator(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth) {
        init(prefix, vfo, audioSampleRate, bandWidth);
    }

    void init(std::string prefix, VFOManager::VFO* vfo, float audioSampleRate, float bandWidth) {
        uiPrefix = prefix;
        _vfo = vfo;
        audioSampRate = audioSampleRate;
        bw = bandWidth;
        
        c2s.init(_vfo->output);
    }

    void start() {
        c2s.start();
        running = true;
    }

    void stop() {
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
    }

    void setVFO(VFOManager::VFO* vfo) {
        _vfo = vfo;
        c2s.setInput(_vfo->output);
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
        if (ImGui::InputFloat(("##_radio_raw_snap_" + uiPrefix).c_str(), &snapInterval, 1, 100, 0)) {
            setSnapInterval(snapInterval);
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

    VFOManager::VFO* _vfo;
    dsp::ComplexToStereo c2s;

};