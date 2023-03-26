#pragma once
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <config.h>
#include <dsp/chain.h>
#include <dsp/noise_reduction/noise_blanker.h>
#include <dsp/noise_reduction/fm_if.h>
#include <dsp/noise_reduction/squelch.h>
#include <dsp/noise_reduction/audio.h>
#include <dsp/multirate/rational_resampler.h>
#include <dsp/filter/deephasis.h>
#include <core.h>
#include <stdint.h>
#include <utils/optionlist.h>
#include "radio_interface.h"
#include "demod.h"

ConfigManager config;

#define CONCAT(a, b) ((std::string(a) + b).c_str())

std::map<DeemphasisMode, double> deempTaus = {
    { DEEMP_MODE_22US, 22e-6 },
    { DEEMP_MODE_50US, 50e-6 },
    { DEEMP_MODE_75US, 75e-6 }
};

std::map<IFNRPreset, double> ifnrTaps = {
    { IFNR_PRESET_NOAA_APT, 9},
    { IFNR_PRESET_VOICE, 15 },
    { IFNR_PRESET_NARROW_BAND, 31 },
    { IFNR_PRESET_BROADCAST, 32 }
};

class RadioModule : public ModuleManager::Instance {
public:
    RadioModule(std::string name) {
        this->name = name;

        // Initialize option lists
        deempModes.define("None", DEEMP_MODE_NONE);
        deempModes.define("22us", DEEMP_MODE_22US);
        deempModes.define("50us", DEEMP_MODE_50US);
        deempModes.define("75us", DEEMP_MODE_75US);

        ifnrPresets.define("NOAA APT", IFNR_PRESET_NOAA_APT);
        ifnrPresets.define("Voice", IFNR_PRESET_VOICE);
        ifnrPresets.define("Narrow Band", IFNR_PRESET_NARROW_BAND);

        // Initialize the config if it doesn't exist
        bool created = false;
        config.acquire();
        if (!config.conf.contains(name)) {
            config.conf[name]["selectedDemodId"] = 1;
            created = true;
        }
        selectedDemodID = config.conf[name]["selectedDemodId"];
        config.release(created);

        // Initialize the VFO
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 200000, 200000, 50000, 200000, false);
        onUserChangedBandwidthHandler.handler = vfoUserChangedBandwidthHandler;
        onUserChangedBandwidthHandler.ctx = this;
        vfo->wtfVFO->onUserChangedBandwidth.bindHandler(&onUserChangedBandwidthHandler);

        // Initialize IF DSP chain
        ifChainOutputChanged.ctx = this;
        ifChainOutputChanged.handler = ifChainOutputChangeHandler;
        ifChain.init(vfo->output);

        nb.init(NULL, 500.0 / 24000.0, 10.0);
        fmnr.init(NULL, 32);
        squelch.init(NULL, MIN_SQUELCH);

        ifChain.addBlock(&nb, false);
        ifChain.addBlock(&squelch, false);
        ifChain.addBlock(&fmnr, false);

        // Initialize audio DSP chain
        afChain.init(&dummyAudioStream);

        resamp.init(NULL, 250000.0, 48000.0);
        deemp.init(NULL, 50e-6, 48000.0);
        afNR.init(NULL, 1024);

        afChain.addBlock(&resamp, true);
        afChain.addBlock(&deemp, false);
        afChain.addBlock(&afNR, false);

        // Initialize the sink
        srChangeHandler.ctx = this;
        srChangeHandler.handler = sampleRateChangeHandler;
        stream.init(afChain.out, &srChangeHandler, audioSampleRate);
        sigpath::sinkManager.registerStream(name, &stream);

        // Select the demodulator
        selectDemodByID((DemodID)selectedDemodID);

        // Start IF chain
        ifChain.start();

        // Start AF chain
        afChain.start();

        // Start stream, the rest was started when selecting the demodulator
        stream.start();

        // Register the menu
        gui::menu.registerEntry(name, menuHandler, this, this);

        // Register the module interface
        core::modComManager.registerInterface("radio", name, moduleInterfaceHandler, this);
    }

    ~RadioModule() {
        core::modComManager.unregisterInterface(name);
        gui::menu.removeEntry(name);
        stream.stop();
        if (enabled) {
            disable();
        }
        sigpath::sinkManager.unregisterStream(name);
    }

    void postInit() {}

    void enable() {
        enabled = true;
        if (!vfo) {
            vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 200000, 200000, 50000, 200000, false);
            vfo->wtfVFO->onUserChangedBandwidth.bindHandler(&onUserChangedBandwidthHandler);
        }
        ifChain.setInput(vfo->output, [=](dsp::stream<dsp::complex_t>* out){ ifChainOutputChangeHandler(out, this); });
        ifChain.start();
        selectDemodByID((DemodID)selectedDemodID);
        afChain.start();
    }

    void disable() {
        enabled = false;
        ifChain.stop();
        if (selectedDemod) { selectedDemod->stop(); }
        afChain.stop();
        if (vfo) { sigpath::vfoManager.deleteVFO(vfo); }
        vfo = NULL;
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

        float menuWidth = ImGui::GetContentRegionAvail().x;
        ImGui::BeginGroup();

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
        if (ImGui::RadioButton(CONCAT("DSB##_", _this->name), _this->selectedDemodID == 3) && _this->selectedDemodID != 3) {
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

        if (!_this->bandwidthLocked) {
            ImGui::LeftLabel("Bandwidth");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::InputFloat(("##_radio_bw_" + _this->name).c_str(), &_this->bandwidth, 1, 100, "%.0f")) {
                _this->bandwidth = std::clamp<float>(_this->bandwidth, _this->minBandwidth, _this->maxBandwidth);
                _this->setBandwidth(_this->bandwidth);
            }
        }

        // VFO snap interval
        ImGui::LeftLabel("Snap Interval");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(("##_radio_snap_" + _this->name).c_str(), &_this->snapInterval, 1, 100)) {
            if (_this->snapInterval < 1) { _this->snapInterval = 1; }
            _this->vfo->setSnapInterval(_this->snapInterval);
            config.acquire();
            config.conf[_this->name][_this->selectedDemod->getName()]["snapInterval"] = _this->snapInterval;
            config.release(true);
        }

        // Deemphasis mode
        if (_this->deempAllowed) {
            ImGui::LeftLabel("De-emphasis");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::Combo(("##_radio_wfm_deemp_" + _this->name).c_str(), &_this->deempId, _this->deempModes.txt)) {
                _this->setDeemphasisMode(_this->deempModes[_this->deempId]);
            }
        }

        // Noise blanker
        if (_this->nbAllowed) {
            if (ImGui::Checkbox(("Noise blanker (W.I.P.)##_radio_nb_ena_" + _this->name).c_str(), &_this->nbEnabled)) {
                _this->setNBEnabled(_this->nbEnabled);
            }
            if (!_this->nbEnabled && _this->enabled) { style::beginDisabled(); }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderFloat(("##_radio_nb_lvl_" + _this->name).c_str(), &_this->nbLevel, _this->MIN_NB, _this->MAX_NB, "%.3fdB")) {
                _this->setNBLevel(_this->nbLevel);
            }
            if (!_this->nbEnabled && _this->enabled) { style::endDisabled(); }
        }
        
        // Noise reduction
        if (_this->afNRAllowed) {
            if (ImGui::Checkbox(("Audio Noise Reduction##_radio_afnr_ena_" + _this->name).c_str(), &_this->afNREnabled)) {
                _this->setAFNREnabled(_this->afNREnabled);
            }
        }

        // Squelch
        if (ImGui::Checkbox(("Squelch##_radio_sqelch_ena_" + _this->name).c_str(), &_this->squelchEnabled)) {
            _this->setSquelchEnabled(_this->squelchEnabled);
        }
        if (!_this->squelchEnabled && _this->enabled) { style::beginDisabled(); }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::SliderFloat(("##_radio_sqelch_lvl_" + _this->name).c_str(), &_this->squelchLevel, _this->MIN_SQUELCH, _this->MAX_SQUELCH, "%.3fdB")) {
            _this->setSquelchLevel(_this->squelchLevel);
        }
        if (!_this->squelchEnabled && _this->enabled) { style::endDisabled(); }

        // FM IF Noise Reduction
        if (_this->FMIFNRAllowed) {
            if (ImGui::Checkbox(("IF Noise Reduction##_radio_fmifnr_ena_" + _this->name).c_str(), &_this->FMIFNREnabled)) {
                _this->setFMIFNREnabled(_this->FMIFNREnabled);
            }
            if (_this->selectedDemodID == RADIO_DEMOD_NFM) {
                if (!_this->FMIFNREnabled && _this->enabled) { style::beginDisabled(); }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
                if (ImGui::Combo(("##_radio_fmifnr_ena_" + _this->name).c_str(), &_this->fmIFPresetId, _this->ifnrPresets.txt)) {
                    _this->setIFNRPreset(_this->ifnrPresets[_this->fmIFPresetId]);
                }
                if (!_this->FMIFNREnabled && _this->enabled) { style::endDisabled(); }
            }
        }

        // Demodulator specific menu
        _this->selectedDemod->showMenu();

        if (!_this->enabled) { style::endDisabled(); }
    }

    demod::Demodulator* instantiateDemod(DemodID id) {
        demod::Demodulator* demod = NULL;
        switch (id) {
            case DemodID::RADIO_DEMOD_NFM:  demod = new demod::NFM(); break;
            case DemodID::RADIO_DEMOD_WFM:  demod = new demod::WFM(); break;
            case DemodID::RADIO_DEMOD_AM:   demod = new demod::AM(); break;
            case DemodID::RADIO_DEMOD_DSB:  demod = new demod::DSB(); break;
            case DemodID::RADIO_DEMOD_USB:  demod = new demod::USB(); break;
            case DemodID::RADIO_DEMOD_CW:   demod = new demod::CW(); break;
            case DemodID::RADIO_DEMOD_LSB:  demod = new demod::LSB(); break;
            case DemodID::RADIO_DEMOD_RAW:  demod = new demod::RAW(); break;
            default:                        demod = NULL; break;
        }
        if (!demod) { return NULL; }

        // Default config
        double bw = demod->getDefaultBandwidth();
        config.acquire();
        if (!config.conf[name].contains(demod->getName())) {
            config.conf[name][demod->getName()]["bandwidth"] = bw;
            config.conf[name][demod->getName()]["snapInterval"] = demod->getDefaultSnapInterval();
            config.conf[name][demod->getName()]["squelchLevel"] = MIN_SQUELCH;
            config.conf[name][demod->getName()]["squelchEnabled"] = false;
            config.release(true);
        }
        else {
            config.release();
        }
        bw = std::clamp<double>(bw, demod->getMinBandwidth(), demod->getMaxBandwidth());

        // Initialize
        demod->init(name, &config, ifChain.out, bw, stream.getSampleRate());

        return demod;
    }

    void selectDemodByID(DemodID id) {
        auto startTime = std::chrono::high_resolution_clock::now();
        demod::Demodulator* demod = instantiateDemod(id);
        if (!demod) {
            flog::error("Demodulator {0} not implemented", (int)id);
            return;
        }
        selectedDemodID = id;
        selectDemod(demod);

        // Save config
        config.acquire();
        config.conf[name]["selectedDemodId"] = id;
        config.release(true);
        auto endTime = std::chrono::high_resolution_clock::now();
        flog::warn("Demod switch took {0} us", (int64_t)((std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime)).count()));
    }

    void selectDemod(demod::Demodulator* demod) {
        // Stopcurrently selected demodulator and select new
        afChain.setInput(&dummyAudioStream, [=](dsp::stream<dsp::stereo_t>* out){ stream.setInput(out); });
        if (selectedDemod) {
            selectedDemod->stop();
            delete selectedDemod;
        }
        selectedDemod = demod;

        // Give the demodulator the most recent audio SR
        selectedDemod->AFSampRateChanged(audioSampleRate);

        // Set the demodulator's input
        selectedDemod->setInput(ifChain.out);

        // Set AF chain's input
        afChain.setInput(selectedDemod->getOutput(), [=](dsp::stream<dsp::stereo_t>* out){ stream.setInput(out); });

        // Load config
        bandwidth = selectedDemod->getDefaultBandwidth();
        minBandwidth = selectedDemod->getMinBandwidth();
        maxBandwidth = selectedDemod->getMaxBandwidth();
        bandwidthLocked = selectedDemod->getBandwidthLocked();
        snapInterval = selectedDemod->getDefaultSnapInterval();
        squelchLevel = MIN_SQUELCH;
        deempAllowed = selectedDemod->getDeempAllowed();
        deempId = deempModes.valueId((DeemphasisMode)selectedDemod->getDefaultDeemphasisMode());
        squelchEnabled = false;
        postProcEnabled = selectedDemod->getPostProcEnabled();
        FMIFNRAllowed = selectedDemod->getFMIFNRAllowed();
        FMIFNREnabled = false;
        fmIFPresetId = ifnrPresets.valueId(IFNR_PRESET_VOICE);
        nbAllowed = selectedDemod->getNBAllowed();
        nbEnabled = false;
        afNRAllowed = selectedDemod->getAFNRAllowed();
        afNREnabled = false;
        nbLevel = 0.0f;
        double ifSamplerate = selectedDemod->getIFSampleRate();
        config.acquire();
        if (config.conf[name][selectedDemod->getName()].contains("bandwidth")) {
            bandwidth = config.conf[name][selectedDemod->getName()]["bandwidth"];
            bandwidth = std::clamp<double>(bandwidth, minBandwidth, maxBandwidth);
        }
        if (config.conf[name][selectedDemod->getName()].contains("snapInterval")) {
            snapInterval = config.conf[name][selectedDemod->getName()]["snapInterval"];
        }
        if (config.conf[name][selectedDemod->getName()].contains("squelchLevel")) {
            squelchLevel = config.conf[name][selectedDemod->getName()]["squelchLevel"];
        }
        if (config.conf[name][selectedDemod->getName()].contains("squelchEnabled")) {
            squelchEnabled = config.conf[name][selectedDemod->getName()]["squelchEnabled"];
        }
        if (config.conf[name][selectedDemod->getName()].contains("deempMode")) {
            if (!config.conf[name][selectedDemod->getName()]["deempMode"].is_string()) {
                config.conf[name][selectedDemod->getName()]["deempMode"] = deempModes.key(deempId);
            }

            std::string deempOpt = config.conf[name][selectedDemod->getName()]["deempMode"];
            if (deempModes.keyExists(deempOpt)) {
                deempId = deempModes.keyId(deempOpt);
            }
        }
        if (config.conf[name][selectedDemod->getName()].contains("FMIFNREnabled")) {
            FMIFNREnabled = config.conf[name][selectedDemod->getName()]["FMIFNREnabled"];
        }
        if (config.conf[name][selectedDemod->getName()].contains("fmifnrPreset")) {
            std::string presetOpt = config.conf[name][selectedDemod->getName()]["fmifnrPreset"];
            if (ifnrPresets.keyExists(presetOpt)) {
                fmIFPresetId = ifnrPresets.keyId(presetOpt);
            }
        }
        if (config.conf[name][selectedDemod->getName()].contains("noiseBlankerEnabled")) {
            nbEnabled = config.conf[name][selectedDemod->getName()]["noiseBlankerEnabled"];
        }
        if (config.conf[name][selectedDemod->getName()].contains("noiseBlankerLevel")) {
            nbLevel = config.conf[name][selectedDemod->getName()]["noiseBlankerLevel"];
        }
        if (config.conf[name][selectedDemod->getName()].contains("audioNoiseReductionEnabled")) {
            nbEnabled = config.conf[name][selectedDemod->getName()]["audioNoiseReductionEnabled"];
        }
        config.release();

        // Configure VFO
        if (vfo) {
            vfo->setBandwidthLimits(minBandwidth, maxBandwidth, selectedDemod->getBandwidthLocked());
            vfo->setReference(selectedDemod->getVFOReference());
            vfo->setSnapInterval(snapInterval);
            vfo->setSampleRate(ifSamplerate, bandwidth);
        }

        // Configure bandwidth
        setBandwidth(bandwidth);

        // Configure noise blanker
        nb.setRate(500.0 / ifSamplerate);
        setNBLevel(nbLevel);
        setNBEnabled(nbAllowed && nbEnabled);

        // Configure FM IF Noise Reduction
        setIFNRPreset((selectedDemodID == RADIO_DEMOD_NFM) ? ifnrPresets[fmIFPresetId] : IFNR_PRESET_BROADCAST);
        setFMIFNREnabled(FMIFNRAllowed ? FMIFNREnabled : false);

        // Configure squelch
        setSquelchLevel(squelchLevel);
        setSquelchEnabled(squelchEnabled);

        // Configure AF chain
        if (postProcEnabled) {
            // Configure resampler
            afChain.stop();
            resamp.setInSamplerate(selectedDemod->getAFSampleRate());
            setAudioSampleRate(audioSampleRate);
            afChain.enableBlock(&resamp, [=](dsp::stream<dsp::stereo_t>* out){ stream.setInput(out); });

            // Configure deemphasis
            setDeemphasisMode(deempAllowed ? deempModes[deempId] : DEEMP_MODE_NONE);

            // Configure AF NR
            setAFNREnabled(afNRAllowed && afNREnabled);
        }
        else {
            // Disable everything if post processing is disabled
            afChain.disableAllBlocks([=](dsp::stream<dsp::stereo_t>* out){ stream.setInput(out); });
        }

        // Start new demodulator
        selectedDemod->start();
    }


    void setBandwidth(double bw) {
        bw = std::clamp<double>(bw, minBandwidth, maxBandwidth);
        bandwidth = bw;
        if (!selectedDemod) { return; }
        vfo->setBandwidth(bandwidth);
        selectedDemod->setBandwidth(bandwidth);

        config.acquire();
        config.conf[name][selectedDemod->getName()]["bandwidth"] = bandwidth;
        config.release(true);
    }

    void setAudioSampleRate(double sr) {
        audioSampleRate = sr;
        if (!selectedDemod) { return; }
        selectedDemod->AFSampRateChanged(audioSampleRate);
        if (!postProcEnabled && vfo) {
            // If postproc is disabled, IF SR = AF SR
            minBandwidth = selectedDemod->getMinBandwidth();
            maxBandwidth = selectedDemod->getMaxBandwidth();
            bandwidth = selectedDemod->getIFSampleRate();
            vfo->setBandwidthLimits(minBandwidth, maxBandwidth, selectedDemod->getBandwidthLocked());
            vfo->setSampleRate(selectedDemod->getIFSampleRate(), bandwidth);
            return;
        }

        afChain.stop();

        // Configure resampler
        resamp.setOutSamplerate(audioSampleRate);

        // Configure deemphasis sample rate
        deemp.setSamplerate(audioSampleRate);

        afChain.start();
    }

    void setDeemphasisMode(DeemphasisMode mode) {
        deempId = deempModes.valueId(mode);
        if (!postProcEnabled || !selectedDemod) { return; }
        bool deempEnabled = (mode != DEEMP_MODE_NONE);
        if (deempEnabled) { deemp.setTau(deempTaus[mode]); }
        afChain.setBlockEnabled(&deemp, deempEnabled, [=](dsp::stream<dsp::stereo_t>* out){ stream.setInput(out); });

        // Save config
        config.acquire();
        config.conf[name][selectedDemod->getName()]["deempMode"] = deempModes.key(deempId);
        config.release(true);
    }

    void setAFNREnabled(bool enable) {
        afNREnabled = enable;
        if (!postProcEnabled || !selectedDemod) { return; }
        afChain.setBlockEnabled(&afNR, afNREnabled, [=](dsp::stream<dsp::stereo_t>* out){ stream.setInput(out); });

        // Save config
        config.acquire();
        config.conf[name][selectedDemod->getName()]["audioNoiseReductionEnabled"] = nbEnabled;
        config.release(true);
    }

    void setNBEnabled(bool enable) {
        nbEnabled = enable;
        if (!selectedDemod) { return; }
        ifChain.setBlockEnabled(&nb, nbEnabled, [=](dsp::stream<dsp::complex_t>* out){ selectedDemod->setInput(out); });

        // Save config
        config.acquire();
        config.conf[name][selectedDemod->getName()]["noiseBlankerEnabled"] = nbEnabled;
        config.release(true);
    }

    void setNBLevel(float level) {
        nbLevel = std::clamp<float>(level, MIN_NB, MAX_NB);
        nb.setLevel(nbLevel);

        // Save config
        config.acquire();
        config.conf[name][selectedDemod->getName()]["noiseBlankerLevel"] = nbLevel;
        config.release(true);
    }

    void setSquelchEnabled(bool enable) {
        squelchEnabled = enable;
        if (!selectedDemod) { return; }
        ifChain.setBlockEnabled(&squelch, squelchEnabled, [=](dsp::stream<dsp::complex_t>* out){ selectedDemod->setInput(out); });

        // Save config
        config.acquire();
        config.conf[name][selectedDemod->getName()]["squelchEnabled"] = squelchEnabled;
        config.release(true);
    }

    void setSquelchLevel(float level) {
        squelchLevel = std::clamp<float>(level, MIN_SQUELCH, MAX_SQUELCH);
        squelch.setLevel(squelchLevel);

        // Save config
        config.acquire();
        config.conf[name][selectedDemod->getName()]["squelchLevel"] = squelchLevel;
        config.release(true);
    }

    void setFMIFNREnabled(bool enabled) {
        FMIFNREnabled = enabled;
        if (!selectedDemod) { return; }
        ifChain.setBlockEnabled(&fmnr, FMIFNREnabled, [=](dsp::stream<dsp::complex_t>* out){ selectedDemod->setInput(out); });

        // Save config
        config.acquire();
        config.conf[name][selectedDemod->getName()]["FMIFNREnabled"] = FMIFNREnabled;
        config.release(true);
    }

    void setIFNRPreset(IFNRPreset preset) {
        // Don't save if in broadcast mode
        if (preset == IFNR_PRESET_BROADCAST) {
            if (!selectedDemod) { return; }
            fmnr.setBins(ifnrTaps[preset]);
            return;
        }

        fmIFPresetId = ifnrPresets.valueId(preset);
        if (!selectedDemod) { return; }
        fmnr.setBins(ifnrTaps[preset]);

        // Save config
        config.acquire();
        config.conf[name][selectedDemod->getName()]["fmifnrPreset"] = ifnrPresets.key(fmIFPresetId);
        config.release(true);
    }

    static void vfoUserChangedBandwidthHandler(double newBw, void* ctx) {
        RadioModule* _this = (RadioModule*)ctx;
        _this->setBandwidth(newBw);
    }

    static void sampleRateChangeHandler(float sampleRate, void* ctx) {
        RadioModule* _this = (RadioModule*)ctx;
        _this->setAudioSampleRate(sampleRate);
    }

    static void ifChainOutputChangeHandler(dsp::stream<dsp::complex_t>* output, void* ctx) {
        RadioModule* _this = (RadioModule*)ctx;
        if (!_this->selectedDemod) { return; }
        _this->selectedDemod->setInput(output);
    }

    static void moduleInterfaceHandler(int code, void* in, void* out, void* ctx) {
        RadioModule* _this = (RadioModule*)ctx;
        if (!_this->enabled || !_this->selectedDemod) { return; }

        // Execute commands
        if (code == RADIO_IFACE_CMD_GET_MODE && out) {
            int* _out = (int*)out;
            *_out = _this->selectedDemodID;
        }
        else if (code == RADIO_IFACE_CMD_SET_MODE && in) {
            int* _in = (int*)in;
            _this->selectDemodByID((DemodID)*_in);
        }
        else if (code == RADIO_IFACE_CMD_GET_BANDWIDTH && out) {
            float* _out = (float*)out;
            *_out = _this->bandwidth;
        }
        else if (code == RADIO_IFACE_CMD_SET_BANDWIDTH && in) {
            float* _in = (float*)in;
            if (_this->bandwidthLocked) { return; }
            _this->setBandwidth(*_in);
        }
        else if (code == RADIO_IFACE_CMD_GET_SQUELCH_ENABLED && out) {
            bool* _out = (bool*)out;
            *_out = _this->squelchEnabled;
        }
        else if (code == RADIO_IFACE_CMD_SET_SQUELCH_ENABLED && in) {
            bool* _in = (bool*)in;
            _this->setSquelchEnabled(*_in);
        }
        else if (code == RADIO_IFACE_CMD_GET_SQUELCH_LEVEL && out) {
            float* _out = (float*)out;
            *_out = _this->squelchLevel;
        }
        else if (code == RADIO_IFACE_CMD_SET_SQUELCH_LEVEL && in) {
            float* _in = (float*)in;
            _this->setSquelchLevel(*_in);
        }
        else {
            return;
        }

        // Success
        return;
    }

    // Handlers
    EventHandler<double> onUserChangedBandwidthHandler;
    EventHandler<float> srChangeHandler;
    EventHandler<dsp::stream<dsp::complex_t>*> ifChainOutputChanged;
    EventHandler<dsp::stream<dsp::stereo_t>*> afChainOutputChanged;

    VFOManager::VFO* vfo = NULL;

    // IF chain
    dsp::chain<dsp::complex_t> ifChain;
    dsp::noise_reduction::NoiseBlanker nb;
    dsp::noise_reduction::FMIF fmnr;
    dsp::noise_reduction::Squelch squelch;

    // Audio chain
    dsp::stream<dsp::stereo_t> dummyAudioStream;
    dsp::chain<dsp::stereo_t> afChain;
    dsp::multirate::RationalResampler<dsp::stereo_t> resamp;
    dsp::filter::Deemphasis<dsp::stereo_t> deemp;
    dsp::noise_reduction::Audio afNR;

    SinkManager::Stream stream;

    demod::Demodulator* selectedDemod = NULL;

    OptionList<std::string, DeemphasisMode> deempModes;
    OptionList<std::string, IFNRPreset> ifnrPresets;

    double audioSampleRate = 48000.0;
    float minBandwidth;
    float maxBandwidth;
    float bandwidth;
    bool bandwidthLocked;
    int snapInterval;
    int selectedDemodID = 1;
    bool postProcEnabled;

    bool squelchEnabled = false;
    float squelchLevel;

    int deempId = 0;
    bool deempAllowed;

    bool afNREnabled = false;
    bool afNRAllowed;

    bool FMIFNRAllowed;
    bool FMIFNREnabled = false;
    int fmIFPresetId;

    bool notchEnabled = false;
    float notchPos = 0;
    float notchWidth = 500;

    bool nbAllowed;
    bool nbEnabled = false;
    float nbLevel = 10.0f;

    const double MIN_NB = 1.0;
    const double MAX_NB = 10.0;
    const double MIN_SQUELCH = -100.0;
    const double MAX_SQUELCH = 0.0;

    bool enabled = true;
};
