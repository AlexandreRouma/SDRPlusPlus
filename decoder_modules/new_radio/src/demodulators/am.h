#pragma once
#include "../demod.h"
#include <dsp/demodulator.h>
#include <dsp/filter.h>

namespace demod {
    class AM : public Demodulator {
    public:
        AM() {}
    
        AM(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, double audioSR) {
            init(name, config, input, bandwidth, outputChangeHandler, audioSR);
        }

        ~AM() {
            stop();
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, double audioSR) {
            this->name = name;
            this->outputChangeHandler = outputChangeHandler;

            // Define structure
            demod.init(input);
            agc.init(&demod.out, 20.0f, getIFSampleRate());
            m2s.init(&agc.out);
        }

        void start() {
            demod.start();
            agc.start();
            m2s.start();
        }

        void stop() {
            demod.stop();
            agc.stop();
            m2s.stop();
        }

        void showMenu() {
            // TODO: Adjust AGC settings
        }

        void setBandwidth(double bandwidth) {}

        void setInput(dsp::stream<dsp::complex_t>* input) {
            demod.setInput(input);
        }

        void AFSampRateChanged(double newSR) {}

        // ============= INFO =============

        const char* getName()                   { return "AM"; }
        double getIFSampleRate()                { return 15000.0; }
        double getAFSampleRate()                { return getIFSampleRate(); }
        double getDefaultBandwidth()            { return 10000.0; }
        double getMinBandwidth()                { return 1000.0; }
        double getMaxBandwidth()                { return getIFSampleRate(); }
        bool getBandwidthLocked()               { return false; }
        double getMaxAFBandwidth()              { return getIFSampleRate() / 2.0; }
        double getDefaultSnapInterval()         { return 1000.0; }
        int getVFOReference()                   { return ImGui::WaterfallVFO::REF_CENTER; }
        bool getDeempAllowed()                  { return false; }
        bool getPostProcEnabled()               { return true; }
        int getDefaultDeemphasisMode()          { return DEEMP_MODE_NONE; }
        double getAFBandwidth(double bandwidth) { return bandwidth / 2.0; }
        bool getDynamicAFBandwidth()            { return true; }
        dsp::stream<dsp::stereo_t>* getOutput() { return &m2s.out; }

    private:
        dsp::AMDemod demod;
        dsp::AGC agc;
        dsp::MonoToStereo m2s;

        std::string name;
        EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler;
    };
}