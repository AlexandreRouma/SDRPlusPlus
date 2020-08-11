#pragma once
#include <dsp/vfo.h>
#include <waterfall.h>
#include <signal_path.h>

namespace vfoman {
    struct VFO_t {
        dsp::VFO* dspVFO;
        ImGui::WaterfallVFO* wtfVFO;
    };

    void init(ImGui::WaterFall* wtf, SignalPath* sigPath);

    dsp::stream<dsp::complex_t>* create(std::string name, int reference, float offset, float bandwidth, float sampleRate, int blockSize);
    void setOffset(std::string name, float offset);
    void setCenterOffset(std::string name, float offset);
    void setBandwidth(std::string name, float bandwidth);
    void setSampleRate(std::string name, float sampleRate, float bandwidth);
    void setReference(std::string name, int ref);
    int getOutputBlockSize(std::string name);
    void remove(std::string name);

    void updateFromWaterfall();
};