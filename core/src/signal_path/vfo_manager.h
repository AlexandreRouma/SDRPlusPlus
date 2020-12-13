#pragma once
#include <dsp/vfo.h>
#include <gui/widgets/waterfall.h>
#include <gui/gui.h>

class VFOManager {
public:
    VFOManager();

    class VFO {
    public:
        VFO(std::string name, int reference, double offset, double bandwidth, double sampleRate, int blockSize);
        ~VFO();

        void setOffset(double offset);
        void setCenterOffset(double offset);
        void setBandwidth(double bandwidth);
        void setSampleRate(double sampleRate, double bandwidth);
        void setReference(int ref);
        int getOutputBlockSize();
        void setSnapInterval(double interval);

        dsp::stream<dsp::complex_t>* output;

        friend class VFOManager;

    private:
        std::string name;
        dsp::VFO* dspVFO;
        ImGui::WaterfallVFO* wtfVFO;
        
    };

    VFOManager::VFO* createVFO(std::string name, int reference, double offset, double bandwidth, double sampleRate, int blockSize);
    void deleteVFO(VFOManager::VFO* vfo);

    void setOffset(std::string name, double offset);
    void setCenterOffset(std::string name, double offset);
    void setBandwidth(std::string name, double bandwidth);
    void setSampleRate(std::string name, double sampleRate, double bandwidth);
    void setReference(std::string name, int ref);
    int getOutputBlockSize(std::string name);

    void updateFromWaterfall(ImGui::WaterFall* wtf);

private:
    std::map<std::string, VFO*> vfos;
};