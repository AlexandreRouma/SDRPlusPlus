#pragma once
#include "../demod.h"
#include <dsp/demod/broadcast_fm.h>
#include <dsp/clock_recovery/mm.h>
#include <dsp/clock_recovery/fd.h>
#include <dsp/taps/root_raised_cosine.h>
#include <dsp/digital/binary_slicer.h>
#include <dsp/digital/manchester_decoder.h>
#include <dsp/digital/differential_decoder.h>
#include <gui/widgets/symbol_diagram.h>
#include <fstream>
#include <rds.h>

namespace demod {
    class WFM : public Demodulator {
    public:
        WFM() {}

        WFM(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            init(name, config, input, bandwidth, audioSR);
        }

        ~WFM() {
            stop();
            gui::waterfall.onFFTRedraw.unbindHandler(&fftRedrawHandler);
        }

        void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) {
            this->name = name;
            _config = config;

            fftRedrawHandler.handler = fftRedraw;
            fftRedrawHandler.ctx = this;
            gui::waterfall.onFFTRedraw.bindHandler(&fftRedrawHandler);

            // Load config
            _config->acquire();
            bool modified = false;
            if (config->conf[name][getName()].contains("stereo")) {
                _stereo = config->conf[name][getName()]["stereo"];
            }
            if (config->conf[name][getName()].contains("lowPass")) {
                _lowPass = config->conf[name][getName()]["lowPass"];
            }
            if (config->conf[name][getName()].contains("rds")) {
                _rds = config->conf[name][getName()]["rds"];
            }
            _config->release(modified);

            // Define structure
            demod.init(input, bandwidth / 2.0f, getIFSampleRate(), _stereo, _lowPass, _rds);
            recov.init(&demod.rdsOut, 5000.0 / 2375, omegaGain, muGain, 0.01);
            slice.init(&recov.out);
            manch.init(&slice.out);
            diff.init(&manch.out, 2);
            hs.init(&diff.out, rdsHandler, this);
        }

        void start() {
            demod.start();
            recov.start();
            slice.start();
            manch.start();
            diff.start();
            hs.start();
        }

        void stop() {
            demod.stop();
            recov.stop();
            slice.stop();
            manch.stop();
            diff.stop();
            hs.stop();
        }

        void showMenu() {
            if (ImGui::Checkbox(("Stereo##_radio_wfm_stereo_" + name).c_str(), &_stereo)) {
                setStereo(_stereo);
                _config->acquire();
                _config->conf[name][getName()]["stereo"] = _stereo;
                _config->release(true);
            }
            if (ImGui::Checkbox(("Low Pass##_radio_wfm_lowpass_" + name).c_str(), &_lowPass)) {
                demod.setLowPass(_lowPass);
                _config->acquire();
                _config->conf[name][getName()]["lowPass"] = _lowPass;
                _config->release(true);
            }
            if (ImGui::Checkbox(("Decode RDS##_radio_wfm_rds_" + name).c_str(), &_rds)) {
                demod.setRDSOut(_rds);
                _config->acquire();
                _config->conf[name][getName()]["rds"] = _rds;
                _config->release(true);
            }

            // if (_rds) {
            //     if (rdsDecode.countryCodeValid()) { ImGui::Text("Country code: %d", rdsDecode.getCountryCode()); }
            //     if (rdsDecode.programCoverageValid()) { ImGui::Text("Program coverage: %d", rdsDecode.getProgramCoverage()); }
            //     if (rdsDecode.programRefNumberValid()) { ImGui::Text("Reference number: %d", rdsDecode.getProgramRefNumber()); }
            //     if (rdsDecode.programTypeValid()) { ImGui::Text("Program type: %d", rdsDecode.getProgramType()); }
            //     if (rdsDecode.PSNameValid()) { ImGui::Text("Program name: [%s]", rdsDecode.getPSName().c_str()); }
            //     if (rdsDecode.radioTextValid()) { ImGui::Text("Radiotext: [%s]", rdsDecode.getRadioText().c_str()); }
            // }
        }

        void setBandwidth(double bandwidth) {
            demod.setDeviation(bandwidth / 2.0f);
        }

        void setInput(dsp::stream<dsp::complex_t>* input) {
            demod.setInput(input);
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
        double getDefaultSnapInterval() { return 100000.0; }
        int getVFOReference() { return ImGui::WaterfallVFO::REF_CENTER; }
        bool getDeempAllowed() { return true; }
        bool getPostProcEnabled() { return true; }
        int getDefaultDeemphasisMode() { return DEEMP_MODE_50US; }
        bool getFMIFNRAllowed() { return true; }
        bool getNBAllowed() { return false; }
        bool getAFNRAllowed() { return false; }
        dsp::stream<dsp::stereo_t>* getOutput() { return &demod.out; }

        // ============= DEDICATED FUNCTIONS =============

        void setStereo(bool stereo) {
            _stereo = stereo;
            demod.setStereo(_stereo);
        }

    private:
        static void rdsHandler(uint8_t* data, int count, void* ctx) {
            WFM* _this = (WFM*)ctx;
            _this->rdsDecode.process(data, count);
        }

        static void fftRedraw(ImGui::WaterFall::FFTRedrawArgs args, void* ctx) {
            WFM* _this = (WFM*)ctx;
            if (!_this->_rds) { return; }

            // Generate string depending on RDS mode
            char buf[256];
            if (_this->rdsDecode.PSNameValid() && _this->rdsDecode.radioTextValid()) {
                sprintf(buf, "RDS: %s - %s", _this->rdsDecode.getPSName().c_str(), _this->rdsDecode.getRadioText().c_str());
            }
            else if (_this->rdsDecode.PSNameValid()) {
                sprintf(buf, "RDS: %s", _this->rdsDecode.getPSName().c_str());
            }
            else if (_this->rdsDecode.radioTextValid()) {
                sprintf(buf, "RDS: %s", _this->rdsDecode.getRadioText().c_str());
            }
            else {
                return;
            }

            // Calculate paddings
            ImVec2 min = args.min;
            min.x += 5.0f * style::uiScale;
            min.y += 5.0f * style::uiScale;
            ImVec2 tmin = min;
            tmin.x += 5.0f * style::uiScale;
            tmin.y += 5.0f * style::uiScale;
            ImVec2 tmax = ImGui::CalcTextSize(buf);
            tmax.x += tmin.x;
            tmax.y += tmin.y;
            ImVec2 max = tmax;
            max.x += 5.0f * style::uiScale;
            max.y += 5.0f * style::uiScale;

            // Draw back drop
            args.window->DrawList->AddRectFilled(min, max, IM_COL32(0, 0, 0, 128));

            // Draw text
            args.window->DrawList->AddText(tmin, IM_COL32(255, 255, 0, 255), buf);
        }

        dsp::demod::BroadcastFM demod;
        dsp::clock_recovery::FD recov;
        dsp::digital::BinarySlicer slice;
        dsp::digital::ManchesterDecoder manch;
        dsp::digital::DifferentialDecoder diff;
        dsp::sink::Handler<uint8_t> hs;
        EventHandler<ImGui::WaterFall::FFTRedrawArgs> fftRedrawHandler;

        rds::RDSDecoder rdsDecode;

        ConfigManager* _config = NULL;

        bool _stereo = false;
        bool _lowPass = true;
        bool _rds = false;
        float muGain = 0.01;
        float omegaGain = (0.01*0.01)/4.0;

        std::string name;
    };
}