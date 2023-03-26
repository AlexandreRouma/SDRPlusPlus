#pragma once
#include "../demod.h"
#include <dsp/demod/fm.h>

namespace demod {
    class NFM : public Demodulator {
    public:
        NFM() {}

        NFM(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            init(name, config, input, bandwidth, audioSR);
        }

        ~NFM() { stop(); }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            this->name = name;
            this->_config = config;

            // Load config
            _config->acquire();
            if (config->conf[name][getName()].contains("lowPass")) {
                _lowPass = config->conf[name][getName()]["lowPass"];
            }
            if (config->conf[name][getName()].contains("highPass")) {
                _highPass = config->conf[name][getName()]["highPass"];
            }
            _config->release();


            // Define structure
            demod.init(input, getIFSampleRate(), bandwidth, _lowPass, _highPass);
        }

        void start() { demod.start(); }

        void stop() { demod.stop(); }

        void showMenu() {
            if (ImGui::Checkbox(("Low Pass##_radio_wfm_lowpass_" + name).c_str(), &_lowPass)) {
                demod.setLowPass(_lowPass);
                _config->acquire();
                _config->conf[name][getName()]["lowPass"] = _lowPass;
                _config->release(true);
            }
            if (ImGui::Checkbox(("High Pass##_radio_wfm_highpass_" + name).c_str(), &_highPass)) {
                demod.setHighPass(_highPass);
                _config->acquire();
                _config->conf[name][getName()]["highPass"] = _highPass;
                _config->release(true);
            }
        }

        void setBandwidth(double bandwidth) {
            demod.setBandwidth(bandwidth);
        }

        void setInput(dsp::stream<dsp::complex_t>* input) { demod.setInput(input); }

        void AFSampRateChanged(double newSR) {}

        // ============= INFO =============

        const char* getName() { return "FM"; }
        double getIFSampleRate() { return 50000.0; }
        double getAFSampleRate() { return getIFSampleRate(); }
        double getDefaultBandwidth() { return 12500.0; }
        double getMinBandwidth() { return 1000.0; }
        double getMaxBandwidth() { return getIFSampleRate(); }
        bool getBandwidthLocked() { return false; }
        double getDefaultSnapInterval() { return 2500.0; }
        int getVFOReference() { return ImGui::WaterfallVFO::REF_CENTER; }
        bool getDeempAllowed() { return true; }
        bool getPostProcEnabled() { return true; }
        int getDefaultDeemphasisMode() { return DEEMP_MODE_NONE; }
        bool getFMIFNRAllowed() { return true; }
        bool getNBAllowed() { return false; }
        bool getAFNRAllowed() { return false; }
        dsp::stream<dsp::stereo_t>* getOutput() { return &demod.out; }

    private:
        dsp::demod::FM<dsp::stereo_t> demod;

        ConfigManager* _config = NULL;

        bool _lowPass = true;
        bool _highPass = false;

        std::string name;
    };
}