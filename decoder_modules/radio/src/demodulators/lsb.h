#pragma once
#include "../demod.h"
#include <dsp/demodulator.h>
#include <dsp/filter.h>

namespace demod {
    class LSB : public Demodulator {
    public:
        LSB() {}

        LSB(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, EventHandler<float> afbwChangeHandler, double audioSR) {
            init(name, config, input, bandwidth, outputChangeHandler, afbwChangeHandler, audioSR);
        }

        ~LSB() {
            stop();
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, EventHandler<float> afbwChangeHandler, double audioSR) {
            this->name = name;

            // Define structure
            demod.init(input, getIFSampleRate(), bandwidth, dsp::SSBDemod::MODE_LSB);
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

        void setBandwidth(double bandwidth) {
            demod.setBandWidth(bandwidth);
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            demod.setInput(input);
        }

        void AFSampRateChanged(double newSR) {}

        // ============= INFO =============

        const char* getName() { return "LSB"; }
        double getIFSampleRate() { return 24000.0; }
        double getAFSampleRate() { return getIFSampleRate(); }
        double getDefaultBandwidth() { return 2800.0; }
        double getMinBandwidth() { return 500.0; }
        double getMaxBandwidth() { return getIFSampleRate() / 2.0; }
        bool getBandwidthLocked() { return false; }
        double getMaxAFBandwidth() { return getIFSampleRate() / 2.0; }
        double getDefaultSnapInterval() { return 100.0; }
        int getVFOReference() { return ImGui::WaterfallVFO::REF_UPPER; }
        bool getDeempAllowed() { return false; }
        bool getPostProcEnabled() { return true; }
        int getDefaultDeemphasisMode() { return DEEMP_MODE_NONE; }
        double getAFBandwidth(double bandwidth) { return bandwidth; }
        bool getDynamicAFBandwidth() { return true; }
        bool getFMIFNRAllowed() { return false; }
        bool getNBAllowed() { return true; }
        dsp::stream<dsp::stereo_t>* getOutput() { return &m2s.out; }

    private:
        dsp::SSBDemod demod;
        dsp::AGC agc;
        dsp::MonoToStereo m2s;

        std::string name;
    };
}