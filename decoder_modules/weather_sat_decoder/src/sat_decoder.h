#pragma once
#include <string>
#include <signal_path/vfo_manager.h>

class SatDecoder {
public:
    virtual void select() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void setVFO(VFOManager::VFO* vfo) = 0;
    virtual bool canRecord() = 0;
    virtual bool startRecording(std::string recPath) { return false; };
    virtual void stopRecording(){};
    virtual bool isRecording() { return false; };
    virtual void drawMenu(float menuWidth) = 0;
};