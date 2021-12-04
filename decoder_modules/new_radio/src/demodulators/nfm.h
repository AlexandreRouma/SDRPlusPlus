#pragma once
#include "../demod.h"
#include <dsp/demodulator.h>
#include <dsp/filter.h>

namespace demod {
    class NFM : public Demodulator {
    public:
        NFM() {}
    
        NFM(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler) {
            init(name, config, input, bandwidth, outputChangeHandler);
        }

        ~NFM() {
            stop();
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler) {
            this->name = name;
            this->outputChangeHandler = outputChangeHandler;

            // Define structure
            demod.init(input, getIFSampleRate(), bandwidth / 2.0f);
        }

        void start() {
            demod.start();
        }

        void stop() {
            demod.stop();
        }

        void showMenu() {}

        void setBandwidth(double bandwidth) {
            demod.setDeviation(bandwidth / 2.0f);
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            demod.setInput(input);
        }

        // ============= INFO =============

        const char* getName()                   { return "FM"; }
        double getIFSampleRate()                { return 50000.0; }
        double getAFSampleRate()                { return getIFSampleRate(); }
        double getDefaultBandwidth()            { return 12500.0; }
        double getMinBandwidth()                { return 1000.0; }
        double getMaxBandwidth()                { return getIFSampleRate(); }
        bool getBandwidthLocked()               { return false; }
        double getMaxAFBandwidth()              { return getIFSampleRate() / 2.0; }
        double getDefaultSnapInterval()         { return 2500.0; }
        int getVFOReference()                   { return ImGui::WaterfallVFO::REF_CENTER; }
        bool getDeempAllowed()                  { return true; }
        int getDefaultDeemphasisMode()          { return DEEMP_MODE_NONE; }
        double getAFBandwidth(double bandwidth) { return bandwidth / 2.0; }
        bool getDynamicAFBandwidth()            { return true; }
        dsp::stream<dsp::stereo_t>* getOutput() { return &demod.out; }

    private:
        dsp::FMDemod demod;

        std::string name;
        EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler;
    };
}