#pragma once
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <config.h>

ConfigManager config;

#define CONCAT(a, b)    ((std::string(a) + b).c_str())

class RadioModule;
#include "demod.h"

class RadioModule : public ModuleManager::Instance {
public:
    RadioModule(std::string name) {
        this->name = name;

        // Initialize the config if it doesn't exist
        config.acquire();
        if (!config.conf.contains(name)) {
            config.conf[name]["selectedDemodId"] = 1;
        }
        selectedDemodID = config.conf[name]["selectedDemodId"];
        config.release(true);

        // Create demodulator instances
        demods.fill(NULL);
        demods[RADIO_DEMOD_WFM] = new demod::WFM();

        // Initialize the VFO
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 200000, 200000, 50000, 200000, false);
        onUserChangedBandwidthHandler.handler = vfoUserChangedBandwidthHandler;
        onUserChangedBandwidthHandler.ctx = this;
        vfo->wtfVFO->onUserChangedBandwidth.bindHandler(&onUserChangedBandwidthHandler);

        // Initialize the sink
        srChangeHandler.ctx = this;
        srChangeHandler.handler = sampleRateChangeHandler;
        stream.init(&deemp.out, &srChangeHandler, audioSampleRate);
        sigpath::sinkManager.registerStream(name, &stream);

        // Load configuration for all demodulators
        for (auto& demod : demods) {
            if (!demod) { continue; }

            // Default config
            double bw = demod->getDefaultBandwidth();
            if (!config.conf[name].contains(demod->getName())) {
                config.conf[name][demod->getName()]["bandwidth"] = bw;
                config.conf[name][demod->getName()]["snapInterval"] = demod->getDefaultSnapInterval();
                config.conf[name][demod->getName()]["squelchLevel"] = MIN_SQUELCH;
            }
            bw = std::clamp<double>(bw, demod->getMinBandwidth(), demod->getMaxBandwidth());
            
            // Initialize
            demod->init(name, &config, &squelch.out, bw);
        }

        // Initialize DSP
        squelch.init(vfo->output, MIN_SQUELCH);
        win.init(24000, 24000, 48000);
        resamp.init(NULL, &win, 250000, 48000);
        deemp.init(&resamp.out, 48000, 50e-6);
        deemp.bypass = false;

        // Select the demodulator
        selectDemodByID((DemodID)selectedDemodID);

        // Start DSP
        squelch.start();
        resamp.start();
        deemp.start();
        stream.start();

        gui::menu.registerEntry(name, menuHandler, this, NULL);
    }

    ~RadioModule() {
        gui::menu.removeEntry(name);
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

    std::string name;

    enum DemodID {
        RADIO_DEMOD_NFM,
        RADIO_DEMOD_WFM,
        RADIO_DEMOD_AM,
        RADIO_DEMOD_DSB,
        RADIO_DEMOD_USB,
        RADIO_DEMOD_CW,
        RADIO_DEMOD_LSB,
        RADIO_DEMOD_RAW,
        _RADIO_DEMOD_COUNT,
    };

private:
    static void menuHandler(void* ctx) {
        RadioModule* _this = (RadioModule*)ctx;

        if (!_this->enabled) { style::beginDisabled(); }

        float menuWidth = ImGui::GetContentRegionAvailWidth();
        ImGui::BeginGroup();

        // TODO: Change VFO ref in signal path

        ImGui::Columns(4, CONCAT("RadioModeColumns##_", _this->name), false);
        if (ImGui::RadioButton(CONCAT("NFM##_", _this->name), _this->selectedDemodID == 0) && _this->selectedDemodID != 0) { 
            _this->selectDemodByID(RADIO_DEMOD_NFM);
        }
        if (ImGui::RadioButton(CONCAT("WFM##_", _this->name), _this->selectedDemodID == 1) && _this->selectedDemodID != 1) {
            _this->selectDemodByID(RADIO_DEMOD_WFM);
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("AM##_", _this->name), _this->selectedDemodID == 2) && _this->selectedDemodID != 2) {
            _this->selectDemodByID(RADIO_DEMOD_AM);
        }
        if (ImGui::RadioButton(CONCAT("DSB##_", _this->name), _this->selectedDemodID == 3) && _this->selectedDemodID != 3)  {
            _this->selectDemodByID(RADIO_DEMOD_DSB);
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("USB##_", _this->name), _this->selectedDemodID == 4) && _this->selectedDemodID != 4) {
            _this->selectDemodByID(RADIO_DEMOD_USB);
        }
        if (ImGui::RadioButton(CONCAT("CW##_", _this->name), _this->selectedDemodID == 5) && _this->selectedDemodID != 5) {
            _this->selectDemodByID(RADIO_DEMOD_CW);
        };
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("LSB##_", _this->name), _this->selectedDemodID == 6) && _this->selectedDemodID != 6) {
            _this->selectDemodByID(RADIO_DEMOD_LSB);
        }
        if (ImGui::RadioButton(CONCAT("RAW##_", _this->name), _this->selectedDemodID == 7) && _this->selectedDemodID != 7) {
            _this->selectDemodByID(RADIO_DEMOD_RAW);
        };
        ImGui::Columns(1, CONCAT("EndRadioModeColumns##_", _this->name), false);

        ImGui::EndGroup();

        _this->selectedDemod->showMenu();

        ImGui::LeftLabel("Squelch");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderFloat(("##_radio_sqelch_" + _this->name).c_str(), &_this->squelchLevel, _this->MIN_SQUELCH, _this->MAX_SQUELCH, "%.3fdB")) {
            _this->squelch.setLevel(_this->squelchLevel);
            config.acquire();
            config.conf[_this->name][_this->selectedDemod->getName()]["squelchLevel"] = _this->squelchLevel;
            config.release(true);
        }

        if (!_this->enabled) { style::endDisabled(); }
    }

    void selectDemodByID(DemodID id) {
        demod::Demodulator* demod = demods[id];
        if (!demod) {
            spdlog::error("Demodulator {0} not implemented", id);
            return;
        }
        selectedDemodID = id;
        selectDemod(demod);
    }

    void selectDemod(demod::Demodulator* demod) {
        // Stopcurrently selected demodulator and select new
        if (selectedDemod) { selectedDemod->stop(); }
        selectedDemod = demod;

        // Load config
        bandwidth = selectedDemod->getDefaultBandwidth();
        double minBandwidth = selectedDemod->getMinBandwidth();
        double maxBandwidth = selectedDemod->getMaxBandwidth();
        snapInterval = selectedDemod->getDefaultSnapInterval();
        squelchLevel = MIN_SQUELCH;
        if (config.conf[name][selectedDemod->getName()].contains("snapInterval")) {
            bandwidth = config.conf[name][selectedDemod->getName()]["bandwidth"];
            bandwidth = std::clamp<double>(bandwidth, minBandwidth, maxBandwidth);
        }
        if (config.conf[name][selectedDemod->getName()].contains("snapInterval")) {
            snapInterval = config.conf[name][selectedDemod->getName()]["snapInterval"];
        }
        if (config.conf[name][selectedDemod->getName()].contains("squelchLevel")) {
            squelchLevel = config.conf[name][selectedDemod->getName()]["squelchLevel"];
        }

        // Configure VFO
        if (vfo) {
            vfo->setBandwidthLimits(minBandwidth, maxBandwidth, false);
            vfo->setReference(selectedDemod->getVFOReference());
            vfo->setSnapInterval(snapInterval);
            vfo->setSampleRate(selectedDemod->getIFSampleRate(), bandwidth);
        }

        // Configure squelch
        squelch.setLevel(squelchLevel);

        // Configure resampler
        resamp.stop();
        resamp.setInput(selectedDemod->getOutput());
        resamp.setInSampleRate(selectedDemod->getAFSampleRate());
        setAudioSampleRate(audioSampleRate);
        resamp.start();

        // Start new demodulator
        selectedDemod->start();
    }

    void setBandwidth(double bw) {
        bandwidth = bw;
        if (!selectedDemod) { return; }
        selectedDemod->setBandwidth(bandwidth);
        config.acquire();
        config.conf[name][selectedDemod->getName()]["bandwidth"] = bandwidth;
        config.release(true);
    }

    void setAudioSampleRate(double sr) {
        audioSampleRate = sr;
        if (!selectedDemod) { return; }
        float audioBW = std::min<float>(selectedDemod->getMaxAFBandwidth(), audioSampleRate / 2.0f);
        resamp.stop();
        resamp.setOutSampleRate(audioSampleRate);
        win.setSampleRate(audioSampleRate * resamp.getInterpolation());
        win.setCutoff(audioBW);
        win.setTransWidth(audioBW);
        resamp.updateWindow(&win);
        resamp.start();
    }

    static void vfoUserChangedBandwidthHandler(double newBw, void* ctx) {
        RadioModule* _this = (RadioModule*)ctx;
        _this->setBandwidth(newBw);
    }

    static void sampleRateChangeHandler(float sampleRate, void* ctx) {
        RadioModule* _this = (RadioModule*)ctx;
        _this->setAudioSampleRate(sampleRate);
    }
    
    EventHandler<double> onUserChangedBandwidthHandler;
    VFOManager::VFO* vfo;
    dsp::Squelch squelch;

    dsp::filter_window::BlackmanWindow win;
    dsp::PolyphaseResampler<dsp::stereo_t> resamp;
    dsp::BFMDeemp deemp;

    EventHandler<float> srChangeHandler;
    SinkManager::Stream stream;

    std::array<demod::Demodulator*, _RADIO_DEMOD_COUNT> demods;
    demod::Demodulator* selectedDemod = NULL;

    double audioSampleRate = 48000.0;
    double bandwidth;
    double snapInterval;
    float squelchLevel;
    int selectedDemodID = 1;

    const double MIN_SQUELCH = -100.0;
    const double MAX_SQUELCH = 0.0;

    bool enabled = true;

};