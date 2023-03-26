#pragma once
#include "../demod.h"
#include <dsp/demod/cw.h>

namespace demod {
    class CW : public Demodulator {
    public:
        CW() {}

        CW(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            init(name, config, input, bandwidth, audioSR);
        }

        ~CW() {
            stop();
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            this->name = name;
            this->_config = config;
            this->afbwChangeHandler = afbwChangeHandler;

            // Load config
            config->acquire();
            if (config->conf[name][getName()].contains("agcAttack")) {
                agcAttack = config->conf[name][getName()]["agcAttack"];
            }
            if (config->conf[name][getName()].contains("agcDecay")) {
                agcDecay = config->conf[name][getName()]["agcDecay"];
            }
            if (config->conf[name][getName()].contains("tone")) {
                tone = config->conf[name][getName()]["tone"];
            }
            config->release();

            // Define structure
            demod.init(input, tone, agcAttack / getIFSampleRate(), agcDecay / getIFSampleRate(), getIFSampleRate());
        }

        void start() { demod.start(); }

        void stop() { demod.stop(); }

        void showMenu() {
            float menuWidth = ImGui::GetContentRegionAvail().x;
            ImGui::LeftLabel("AGC Attack");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderFloat(("##_radio_cw_agc_attack_" + name).c_str(), &agcAttack, 1.0f, 200.0f)) {
                demod.setAGCAttack(agcAttack / getIFSampleRate());
                _config->acquire();
                _config->conf[name][getName()]["agcAttack"] = agcAttack;
                _config->release(true);
            }
            ImGui::LeftLabel("AGC Decay");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderFloat(("##_radio_cw_agc_decay_" + name).c_str(), &agcDecay, 1.0f, 20.0f)) {
                demod.setAGCDecay(agcDecay / getIFSampleRate());
                _config->acquire();
                _config->conf[name][getName()]["agcDecay"] = agcDecay;
                _config->release(true);
            }
            ImGui::LeftLabel("Tone Frequency");
            ImGui::FillWidth();
            if (ImGui::InputInt(("Stereo##_radio_cw_tone_" + name).c_str(), &tone, 10, 100)) {
                tone = std::clamp<int>(tone, 250, 1250);
                demod.setTone(tone);
                _config->acquire();
                _config->conf[name][getName()]["tone"] = tone;
                _config->release(true);
            }
        }

        void setBandwidth(double bandwidth) {}

        void setInput(dsp::stream<dsp::complex_t>* input) { demod.setInput(input); }

        void AFSampRateChanged(double newSR) {}

        // ============= INFO =============

        const char* getName() { return "CW"; }
        double getIFSampleRate() { return 3000.0; }
        double getAFSampleRate() { return getIFSampleRate(); }
        double getDefaultBandwidth() { return 200.0; }
        double getMinBandwidth() { return 50.0; }
        double getMaxBandwidth() { return 500.0; }
        bool getBandwidthLocked() { return false; }
        double getDefaultSnapInterval() { return 10.0; }
        int getVFOReference() { return ImGui::WaterfallVFO::REF_CENTER; }
        bool getDeempAllowed() { return false; }
        bool getPostProcEnabled() { return true; }
        int getDefaultDeemphasisMode() { return DEEMP_MODE_NONE; }
        bool getFMIFNRAllowed() { return false; }
        bool getNBAllowed() { return false; }
        bool getAFNRAllowed() { return false; }
        dsp::stream<dsp::stereo_t>* getOutput() { return &demod.out; }

    private:
        ConfigManager* _config = NULL;
        dsp::demod::CW<dsp::stereo_t> demod;

        std::string name;

        float agcAttack = 100.0f;
        float agcDecay = 5.0f;
        int tone = 800;

        EventHandler<float> afbwChangeHandler;
    };
}