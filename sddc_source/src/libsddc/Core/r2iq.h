#ifndef R2IQ_H
#define R2IQ_H
#include "license.txt" 

#define NDECIDX 7  //number of srate

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "dsp/ringbuffer.h"

struct r2iqThreadArg;

class r2iqControlClass {
public:
    r2iqControlClass();
    virtual ~r2iqControlClass() {}

    int getRatio()  {return mratio [mdecimation];}

    void updateRand(bool v) { this->randADC = v; }
    bool getRand() const { return this->randADC; }

    void setSideband(bool lsb) { this->sideband = lsb; }
    bool getSideband() const { return this->sideband; }

    void setDecimate(int dec) {this->mdecimation = dec; }

    virtual void Init(float gain, ringbuffer<int16_t>* input, ringbuffer<float>* obuffers) {}
    virtual void TurnOn() { this->r2iqOn = true; }
    virtual void TurnOff(void) { this->r2iqOn = false; }
    virtual bool IsOn(void) { return this->r2iqOn; }
    virtual void DataReady(void) {}
    virtual float setFreqOffset(float offset) { return 0; };

protected:
    int mdecimation ;   // selected decimation ratio
      // 64 Msps:               0 => 32Msps, 1=> 16Msps, 2 = 8Msps, 3 = 4Msps, 4 = 2Msps
      // 128 Msps: 0 => 64Msps, 1 => 32Msps, 2=> 16Msps, 3 = 8Msps, 4 = 4Msps, 5 = 2Msps
    bool r2iqOn;        // r2iq on flag
    int mratio [NDECIDX];  // ratio

private:
    bool randADC;       // randomized ADC output
    bool sideband;
};

#endif
