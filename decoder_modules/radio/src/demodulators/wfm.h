#pragma once
#include "../demod.h"
#include <dsp/demodulator.h>
#include <dsp/filter.h>

namespace demod {
    class WFM : public Demodulator {
    public:
        WFM() {}

        WFM(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, double audioSR) {
            init(name, config, input, bandwidth, outputChangeHandler, audioSR);
        }

        ~WFM() {
            stop();
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, double audioSR) {
            this->name = name;
            this->outputChangeHandler = outputChangeHandler;
            _config = config;

            // Load config
            _config->acquire();
            bool modified = false;
            if (config->conf[name][getName()].contains("stereo")) {
                stereo = config->conf[name][getName()]["stereo"];
            }
            _config->release(modified);

            // Define structure
            demod.init(input, getIFSampleRate(), bandwidth / 2.0f);
            demodStereo.init(input, getIFSampleRate(), bandwidth / 2.0f);
        }

        void start() {
            stereo ? demodStereo.start() : demod.start();
        }

        void stop() {
            demod.stop();
            demodStereo.stop();
        }

        void showMenu() {
            if (ImGui::Checkbox(("Stereo##_radio_wfm_stereo_" + name).c_str(), &stereo)) {
                setStereo(stereo);
                _config->acquire();
                _config->conf[name][getName()]["stereo"] = stereo;
                _config->release(true);
            }
        }

        void setBandwidth(double bandwidth) {
            demod.setDeviation(bandwidth / 2.0f);
            demodStereo.setDeviation(bandwidth / 2.0f);
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            demod.setInput(input);
            demodStereo.setInput(input);
        }

        void AFSampRateChanged(double newSR) {}

        // ============= INFO =============

        const char* getName() { return "WFM"; }
        double getIFSampleRate() { return 250000.0; }
        double getAFSampleRate() { return getIFSampleRate(); }
        double getDefaultBandwidth() { return 150000.0; }
        double getMinBandwidth() { return 50000.0; }
        double getMaxBandwidth() { return getIFSampleRate(); }
        bool getBandwidthLocked() { return false; }
        double getMaxAFBandwidth() { return 16000.0; }
        double getDefaultSnapInterval() { return 100000.0; }
        int getVFOReference() { return ImGui::WaterfallVFO::REF_CENTER; }
        bool getDeempAllowed() { return true; }
        bool getPostProcEnabled() { return true; }
        int getDefaultDeemphasisMode() { return DEEMP_MODE_50US; }
        double getAFBandwidth(double bandwidth) { return 16000.0; }
        bool getDynamicAFBandwidth() { return false; }
        bool getFMIFNRAllowed() { return true; }
        bool getNBAllowed() { return false; }
        dsp::stream<dsp::stereo_t>* getOutput() { return stereo ? demodStereo.out : &demod.out; }

        // ============= DEDICATED FUNCTIONS =============

        void setStereo(bool _stereo) {
            stereo = _stereo;
            if (stereo) {
                demod.stop();
                outputChangeHandler.handler(demodStereo.out, outputChangeHandler.ctx);
                demodStereo.start();
            }
            else {
                demodStereo.stop();
                outputChangeHandler.handler(&demod.out, outputChangeHandler.ctx);
                demod.start();
            }
        }

    private:
        dsp::FMDemod demod;
        dsp::StereoFMDemod demodStereo;

        ConfigManager* _config = NULL;

        bool stereo = false;

        std::string name;
        EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler;
    };
}