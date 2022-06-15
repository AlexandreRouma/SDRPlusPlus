#pragma once
#include "../demod.h"
#include <dsp/demod/ssb.h>
#include <dsp/convert/mono_to_stereo.h>

namespace demod {
    class USB : public Demodulator {
    public:
        USB() {}

        USB(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, EventHandler<float> afbwChangeHandler, double audioSR) {
            init(name, config, input, bandwidth, outputChangeHandler, afbwChangeHandler, audioSR);
        }

        ~USB() {
            stop();
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, EventHandler<float> afbwChangeHandler, double audioSR) {
            this->name = name;

            // Define structure
            demod.init(input, dsp::demod::SSB::Mode::USB, bandwidth, getIFSampleRate(), 200000.0 / getIFSampleRate());
            m2s.init(&demod.out);
        }

        void start() {
            demod.start();
            m2s.start();
        }

        void stop() {
            demod.stop();
            m2s.stop();
        }

        void showMenu() {
            // TODO: Adjust AGC settings
        }

        void setBandwidth(double bandwidth) {
            demod.setBandwidth(bandwidth);
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            demod.setInput(input);
        }

        void AFSampRateChanged(double newSR) {}

        // ============= INFO =============

        const char* getName() { return "USB"; }
        double getIFSampleRate() { return 24000.0; }
        double getAFSampleRate() { return getIFSampleRate(); }
        double getDefaultBandwidth() { return 2800.0; }
        double getMinBandwidth() { return 500.0; }
        double getMaxBandwidth() { return getIFSampleRate() / 2.0; }
        bool getBandwidthLocked() { return false; }
        double getMaxAFBandwidth() { return getIFSampleRate() / 2.0; }
        double getDefaultSnapInterval() { return 100.0; }
        int getVFOReference() { return ImGui::WaterfallVFO::REF_LOWER; }
        bool getDeempAllowed() { return false; }
        bool getPostProcEnabled() { return true; }
        int getDefaultDeemphasisMode() { return DEEMP_MODE_NONE; }
        double getAFBandwidth(double bandwidth) { return bandwidth; }
        bool getDynamicAFBandwidth() { return true; }
        bool getFMIFNRAllowed() { return false; }
        bool getNBAllowed() { return true; }
        dsp::stream<dsp::stereo_t>* getOutput() { return &m2s.out; }

    private:
        dsp::demod::SSB demod;
        dsp::convert::MonoToStereo m2s;

        std::string name;
    };
}