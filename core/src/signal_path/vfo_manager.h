#pragma once
#include <dsp/vfo.h>
#include <gui/waterfall.h>
#include <gui/gui.h>

class VFOManager {
public:
    VFOManager();

    class VFO {
    public:
        VFO(std::string name, int reference, float offset, float bandwidth, float sampleRate, int blockSize);
        ~VFO();

        void setOffset(float offset);
        void setCenterOffset(float offset);
        void setBandwidth(float bandwidth);
        void setSampleRate(float sampleRate, float bandwidth);
        void setReference(int ref);
        int getOutputBlockSize();

        dsp::stream<dsp::complex_t>* output;

        friend class VFOManager;

    private:
        std::string name;
        dsp::VFO* dspVFO;
        ImGui::WaterfallVFO* wtfVFO;
        
    };

    VFOManager::VFO* createVFO(std::string name, int reference, float offset, float bandwidth, float sampleRate, int blockSize);
    void deleteVFO(VFOManager::VFO* vfo);

    void setOffset(std::string name, float offset);
    void setCenterOffset(std::string name, float offset);
    void setBandwidth(std::string name, float bandwidth);
    void setSampleRate(std::string name, float sampleRate, float bandwidth);
    void setReference(std::string name, int ref);
    int getOutputBlockSize(std::string name);

    void updateFromWaterfall(ImGui::WaterFall* wtf);

private:
    std::map<std::string, VFO*> vfos;
};