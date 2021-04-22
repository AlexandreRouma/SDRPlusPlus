#pragma once
#include <dsp/vfo.h>
#include <gui/widgets/waterfall.h>
#include <gui/gui.h>

class VFOManager {
public:
    VFOManager();

    class VFO {
    public:
        VFO(std::string name, int reference, double offset, double bandwidth, double sampleRate, double minBandwidth, double maxBandwidth, bool bandwidthLocked);
        ~VFO();

        void setOffset(double offset);
        double getOffset();
        void setCenterOffset(double offset);
        void setBandwidth(double bandwidth, bool updateWaterfall = true);
        void setSampleRate(double sampleRate, double bandwidth);
        void setReference(int ref);
        void setSnapInterval(double interval);
        void setBandwidthLimits(double minBandwidth, double maxBandwidth, bool bandwidthLocked);
        bool getBandwidthChanged(bool erase = true);
        double getBandwidth();

        dsp::stream<dsp::complex_t>* output;

        friend class VFOManager;

    private:
        std::string name;
        dsp::VFO* dspVFO;
        ImGui::WaterfallVFO* wtfVFO;
        
    };

    VFOManager::VFO* createVFO(std::string name, int reference, double offset, double bandwidth, double sampleRate, double minBandwidth, double maxBandwidth, bool bandwidthLocked);
    void deleteVFO(VFOManager::VFO* vfo);

    void setOffset(std::string name, double offset);
    double getOffset(std::string name);
    void setCenterOffset(std::string name, double offset);
    void setBandwidth(std::string name, double bandwidth, bool updateWaterfall = true);
    void setSampleRate(std::string name, double sampleRate, double bandwidth);
    void setReference(std::string name, int ref);
    void setBandwidthLimits(std::string name, double minBandwidth, double maxBandwidth, bool bandwidthLocked);
    bool getBandwidthChanged(std::string name, bool erase = true);
    double getBandwidth(std::string name);

    void updateFromWaterfall(ImGui::WaterFall* wtf);

private:
    std::map<std::string, VFO*> vfos;
};