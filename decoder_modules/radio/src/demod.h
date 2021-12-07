#pragma once
#include <dsp/stream.h>
#include <dsp/types.h>
#include <gui/widgets/waterfall.h>
#include <config.h>
#include <utils/event.h>

enum DeemphasisMode {
    DEEMP_MODE_50US,
    DEEMP_MODE_75US,
    DEEMP_MODE_NONE,
    _DEEMP_MODE_COUNT
};

namespace demod {
    class Demodulator {
    public:
        virtual ~Demodulator() {}
        virtual void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, double audioSR) = 0;
        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void showMenu() = 0;
        virtual void setBandwidth(double bandwidth) = 0;
        virtual void setInput(dsp::stream<dsp::complex_t>* input) = 0;
        virtual void AFSampRateChanged(double newSR) = 0;
        virtual const char* getName() = 0;
        virtual double getIFSampleRate() = 0;
        virtual double getAFSampleRate() = 0;
        virtual double getDefaultBandwidth() = 0;
        virtual double getMinBandwidth() = 0;
        virtual double getMaxBandwidth() = 0;
        virtual bool getBandwidthLocked() = 0;
        virtual double getMaxAFBandwidth() = 0;
        virtual double getDefaultSnapInterval() = 0;
        virtual int getVFOReference() = 0;
        virtual bool getDeempAllowed() = 0;
        virtual bool getPostProcEnabled() = 0;
        virtual int getDefaultDeemphasisMode() = 0;
        virtual double getAFBandwidth(double bandwidth) = 0;
        
        virtual bool getDynamicAFBandwidth() = 0;
        virtual dsp::stream<dsp::stereo_t>* getOutput() = 0;
    };
}

#include "demodulators/wfm.h"
#include "demodulators/nfm.h"
#include "demodulators/am.h"
#include "demodulators/usb.h"
#include "demodulators/lsb.h"
#include "demodulators/dsb.h"
#include "demodulators/cw.h"
#include "demodulators/raw.h"