#pragma once
#include "../demod.h"
#include <dsp/demod/broadcast_fm.h>
#include <dsp/clock_recovery/mm.h>
#include <dsp/loop/fast_agc.h>
#include <dsp/loop/costas.h>
#include <dsp/taps/root_raised_cosine.h>
#include <dsp/digital/binary_slicer.h>
#include <dsp/digital/manchester_decoder.h>
#include <dsp/digital/differential_decoder.h>
#include <dsp/routing/doubler.h>
#include <gui/widgets/symbol_diagram.h>
#include <fstream>
#include <rds.h>

namespace demod {
    class WFM : public Demodulator {
    public:
        WFM() : diag(0.5, 4096)  {}

        WFM(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) : diag(0.5, 4096) {
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
            if (config->conf[name][getName()].contains("rdsInfo")) {
                _rdsInfo = config->conf[name][getName()]["rdsInfo"];
            }
            _config->release(modified);

            // Define structure
            demod.init(input, bandwidth / 2.0f, getIFSampleRate(), _stereo, _lowPass, _rds);
            agc.init(&demod.rdsOut, 1.0, 1e6, 0.1);
            costas.init(&agc.out,  0.005f);

            taps = dsp::taps::bandPass<dsp::complex_t>(0, 2375, 100, 5000);
            fir.init(&costas.out, taps);
            double baudfreq = dsp::math::hzToRads(2375.0/2.0, 5000);
            costas2.init(&fir.out, 0.01, 0.0, baudfreq, baudfreq - (baudfreq*0.1), baudfreq + (baudfreq*0.1));

            c2r.init(&costas2.out);
            recov.init(&c2r.out, 5000.0 / (2375.0 / 2.0), 1e-6, 0.01, 0.01);
            slice.init(&doubler.outA);
            diff.init(&slice.out, 2);
            hs.init(&diff.out, rdsHandler, this);

            doubler.init(&recov.out);
            reshape.init(&doubler.outB, 4096, (1187 / 30) - 4096);
            diagHandler.init(&reshape.out, _diagHandler, this);
            diag.lines.push_back(-0.8);
            diag.lines.push_back(0.8);
        }

        void start() {
            agc.start();
            costas.start();
            fir.start();
            costas2.start();
            c2r.start();
            demod.start();
            recov.start();
            slice.start();
            diff.start();
            hs.start();

            doubler.start();
            reshape.start();
            diagHandler.start();
        }

        void stop() {
            agc.stop();
            costas.stop();
            fir.stop();
            costas2.stop();
            c2r.stop();
            demod.stop();
            recov.stop();
            slice.stop();
            diff.stop();
            hs.stop();

            c2r.stop();
            reshape.stop();
            diagHandler.stop();
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

            // TODO: This will break when the entire radio module is disabled
            if (!_rds) { ImGui::BeginDisabled(); }
            if (ImGui::Checkbox(("Advanced RDS Info##_radio_wfm_rds_info_" + name).c_str(), &_rdsInfo)) {
                _config->acquire();
                _config->conf[name][getName()]["rdsInfo"] = _rdsInfo;
                _config->release(true);
            }
            if (!_rds) { ImGui::EndDisabled(); }

            float menuWidth = ImGui::GetContentRegionAvail().x;

            if (_rds && _rdsInfo) {
                ImGui::BeginTable(("##radio_wfm_rds_info_tbl_" + name).c_str(), 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders);
                if (rdsDecode.piCodeValid()) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("PI Code");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("0x%04X (%s)", rdsDecode.getPICode(), rdsDecode.getCallsign().c_str());
                    
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Country Code");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", rdsDecode.getCountryCode());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Program Coverage");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s (%d)", rds::AREA_COVERAGE_TO_STR[rdsDecode.getProgramCoverage()], rdsDecode.getProgramCoverage());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Reference Number");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", rdsDecode.getProgramRefNumber());
                }
                else {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("PI Code");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted("0x---- (----)");
                    
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Country Code");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted("--");  // TODO: String

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Program Coverage");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted("------- (--)");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Reference Number");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted("--");
                }

                if (rdsDecode.programTypeValid()) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Program Type");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s (%d)", rds::PROGRAM_TYPE_US_TO_STR[rdsDecode.getProgramType()], rdsDecode.getProgramType());
                }
                else {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Program Type");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted("------- (--)");  // TODO: String
                }

                if (rdsDecode.musicValid()) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Music");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", rdsDecode.getMusic() ? "Yes":"No");
                }
                else {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Music");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted("---");
                }

                ImGui::EndTable();

                ImGui::SetNextItemWidth(menuWidth);
                diag.draw();
            }
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

        // DEBUGGING ONLY
        static void _diagHandler(float* data, int count, void* ctx) {
            WFM* _this = (WFM*)ctx;
            float* buf = _this->diag.acquireBuffer();
            memcpy(buf, data, count * sizeof(float));
            _this->diag.releaseBuffer();
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
        dsp::loop::FastAGC<dsp::complex_t> agc;
        dsp::loop::Costas<2> costas;
        dsp::tap<dsp::complex_t> taps;
        dsp::filter::FIR<dsp::complex_t, dsp::complex_t> fir;
        dsp::loop::Costas<2> costas2;
        dsp::convert::ComplexToReal c2r;
        dsp::clock_recovery::MM<float> recov;
        dsp::digital::BinarySlicer slice;
        dsp::digital::DifferentialDecoder diff;
        dsp::sink::Handler<uint8_t> hs;
        EventHandler<ImGui::WaterFall::FFTRedrawArgs> fftRedrawHandler;

        dsp::routing::Doubler<float> doubler;
        dsp::buffer::Reshaper<float> reshape;
        dsp::sink::Handler<float> diagHandler;
        ImGui::SymbolDiagram diag;

        rds::RDSDecoder rdsDecode;

        ConfigManager* _config = NULL;

        bool _stereo = false;
        bool _lowPass = true;
        bool _rds = false;
        bool _rdsInfo = false;
        float muGain = 0.01;
        float omegaGain = (0.01*0.01)/4.0;

        std::string name;
    };
}