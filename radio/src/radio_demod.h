#pragma once
#include <dsp/stream.h>
#include <signal_path/vfo_manager.h>

class Demodulator {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() = 0;
    virtual void select() = 0;
    virtual void setVFO(VFOManager::VFO* vfo) = 0;
    virtual VFOManager::VFO* getVFO() = 0;
    virtual void setAudioSampleRate(float sampleRate) = 0;
    virtual float getAudioSampleRate() = 0;
    virtual void setBandwidth(float bandWidth, bool updateWaterfall = true) = 0;
    virtual dsp::stream<dsp::stereo_t>* getOutput() = 0;
    virtual void showMenu() = 0;
    virtual void saveParameters(bool lock = true) = 0;
};