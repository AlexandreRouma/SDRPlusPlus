#pragma once
#include "../demod.h"
#include <dsp/demodulator.h>
#include <dsp/filter.h>

namespace demod {
    class WFM : public Demodulator {
    public:
        WFM() {}
    
        WFM(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth) {
            init(name, config, input, bandwidth);
        }

        ~WFM() {
            stop();
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth) {
            this->name = name;
            _config = config;

            // Load config
            _config->acquire();
            bool modified =false;
            if (!config->conf[name].contains(getName())) {
                config->conf[name][getName()]["deempMode"] = 0;
                config->conf[name][getName()]["stereo"] = false;
                modified = true;
            }
            if (config->conf[name][getName()].contains("deempMode")) {
                deempMode = config->conf[name][getName()]["deempMode"];
            }
            if (config->conf[name][getName()].contains("stereo")) {
                stereo = config->conf[name][getName()]["stereo"];
            }
            _config->release(modified);

            // Define structure
            demod.init(input, getIFSampleRate(), bandwidth / 2.0f);
            demodStereo.init(input, getIFSampleRate(), bandwidth / 2.0f);
            //deemp.init(stereo ? demodStereo.out : &demod.out, getAFSampleRate(), 50e-6);
            //deemp.bypass = false;
        }

        void start() {
            stereo ? demodStereo.start() : demod.start();
            //deemp.start();
        }

        void stop() {
            demod.stop();
            demodStereo.stop();
            //deemp.stop();
        }

        void showMenu() {
            if (ImGui::Checkbox(("Stereo##_radio_wfm_stereo_" + name).c_str(), &stereo)) {
                setStereo(stereo);
                _config->acquire();
                _config->conf[name][getName()]["stereo"] = stereo;
                _config->release(true);
            }
            //ImGui::Checkbox("Deemp bypass", &deemp.bypass);
        }

        void setBandwidth(double bandwidth) {
            demod.setDeviation(bandwidth / 2.0f);
            demodStereo.setDeviation(bandwidth / 2.0f);
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            demod.setInput(input);
            demodStereo.setInput(input);
        }

        // ============= INFO =============

        const char* getName()                   { return "WFM"; }
        double getIFSampleRate()                { return 250000.0; }
        double getAFSampleRate()                { return getIFSampleRate(); }
        double getDefaultBandwidth()            { return 150000.0; }
        double getMinBandwidth()                { return 50000.0; }
        double getMaxBandwidth()                { return getIFSampleRate(); }
        double getMaxAFBandwidth()              { return 16000.0; }
        double getDefaultSnapInterval()         { return 100000.0; }
        int getVFOReference()                   { return ImGui::WaterfallVFO::REF_CENTER; }
        dsp::stream<dsp::stereo_t>* getOutput() { return demodStereo.out; }

        // ============= DEDICATED FUNCTIONS =============

        void setStereo(bool _stereo) {
            stereo = _stereo;
            if (stereo) {
                demod.stop();
                //deemp.setInput(demodStereo.out);
                demodStereo.start();
            }
            else {
                demodStereo.stop();
                //deemp.setInput(&demod.out);
                demod.start();
            }
        }

    private:
        dsp::FMDemod demod;
        dsp::StereoFMDemod demodStereo;
        //dsp::BFMDeemp deemp;
        
        RadioModule* _rad = NULL;
        ConfigManager* _config = NULL;
        
        int deempMode;
        bool stereo;

        std::string name;

    };
}