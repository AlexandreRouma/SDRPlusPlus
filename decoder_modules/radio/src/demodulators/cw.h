#pragma once
#include "../demod.h"
#include <dsp/channel/frequency_xlator.h>
#include <dsp/convert/complex_to_real.h>
#include <dsp/convert/mono_to_stereo.h>
#include <dsp/loop/agc.h>

namespace demod {
    class CW : public Demodulator {
    public:
        CW() {}

        CW(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, EventHandler<float> afbwChangeHandler, double audioSR) {
            init(name, config, input, bandwidth, outputChangeHandler, afbwChangeHandler, audioSR);
        }

        ~CW() {
            stop();
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, EventHandler<dsp::stream<dsp::stereo_t>*> outputChangeHandler, EventHandler<float> afbwChangeHandler, double audioSR) {
            this->name = name;
            this->_config = config;
            this->_bandwidth = bandwidth;
            this->afbwChangeHandler = afbwChangeHandler;

            // Load config
            config->acquire();
            if (config->conf[name][getName()].contains("tone")) {
                tone = config->conf[name][getName()]["tone"];
            }
            config->release();

            // Define structure
            xlator.init(input, tone, getIFSampleRate());
            c2r.init(&xlator.out);
            agc.init(&c2r.out, 1.0, 200000.0 / getIFSampleRate());
            m2s.init(&agc.out);
        }

        void start() {
            xlator.start();
            c2r.start();
            agc.start();
            m2s.start();
        }

        void stop() {
            xlator.stop();
            c2r.stop();
            agc.stop();
            m2s.stop();
        }

        void showMenu() {
            ImGui::LeftLabel("Tone Frequency");
            ImGui::FillWidth();
            if (ImGui::InputInt(("Stereo##_radio_cw_tone_" + name).c_str(), &tone, 10, 100)) {
                tone = std::clamp<int>(tone, 250, 1250);
                xlator.setOffset(tone, getIFSampleRate());
                afbwChangeHandler.handler(getAFBandwidth(_bandwidth), afbwChangeHandler.ctx);
                _config->acquire();
                _config->conf[name][getName()]["tone"] = tone;
                _config->release(true);
            }
        }

        void setBandwidth(double bandwidth) { _bandwidth = bandwidth; }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            xlator.setInput(input);
        }

        void AFSampRateChanged(double newSR) {}

        // ============= INFO =============

        const char* getName() { return "CW"; }
        double getIFSampleRate() { return 3000.0; }
        double getAFSampleRate() { return getIFSampleRate(); }
        double getDefaultBandwidth() { return 500.0; }
        double getMinBandwidth() { return 50.0; }
        double getMaxBandwidth() { return 500.0; }
        bool getBandwidthLocked() { return false; }
        double getMaxAFBandwidth() { return getIFSampleRate() / 2.0; }
        double getDefaultSnapInterval() { return 10.0; }
        int getVFOReference() { return ImGui::WaterfallVFO::REF_CENTER; }
        bool getDeempAllowed() { return false; }
        bool getPostProcEnabled() { return true; }
        int getDefaultDeemphasisMode() { return DEEMP_MODE_NONE; }
        double getAFBandwidth(double bandwidth) { return (bandwidth / 2.0) + (float)tone; }
        bool getDynamicAFBandwidth() { return true; }
        bool getFMIFNRAllowed() { return false; }
        bool getNBAllowed() { return false; }
        dsp::stream<dsp::stereo_t>* getOutput() { return &m2s.out; }

    private:
        ConfigManager* _config = NULL;
        dsp::channel::FrequencyXlator xlator;
        dsp::convert::ComplexToReal c2r;
        dsp::loop::AGC<float> agc;
        dsp::convert::MonoToStereo m2s;

        std::string name;

        int tone = 800;
        double _bandwidth;

        EventHandler<float> afbwChangeHandler;
    };
}