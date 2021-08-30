#include <SoapySDR/Constants.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/widgets/stepped_slider.h>
#include <signal_path/signal_path.h>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Logger.hpp>
#include <core.h>
#include <gui/style.h>
#include <options.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "soapy_source",
    /* Description:     */ "SoapySDR source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 5,
    /* Max instances    */ 1
};

ConfigManager config;

class SoapyModule : public ModuleManager::Instance {
public:
    SoapyModule(std::string name) {
        this->name = name;

        //TODO: Make module tune on source select change (in sdrpp_core)

        uiGains = new float[1];

        refresh();

        // Select default device
        config.acquire();
        std::string devName = config.conf["device"];
        config.release();
        selectDevice(devName);

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("SoapySDR", &handler);
    }

    ~SoapyModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("SoapySDR");
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
    
    template <typename T>
    std::string to_string_with_precision(const T a_value, const int n = 6) {
        std::ostringstream out;
        out.precision(n);
        out << std::fixed << a_value;
        return out.str();
    }

private:
    void refresh() {
        devList = SoapySDR::Device::enumerate();
        txtDevList = "";
        int i = 0;
        for (auto& dev : devList) {
            txtDevList += dev["label"] != "" ? dev["label"] : dev["driver"];
            txtDevList += '\0';
            i++;
        }
    }
    
    float selectBwBySr(double samplerate) {
        float cur = bandwidthList[1];
        std::vector<float> bwListReversed = bandwidthList;
        std::reverse(bwListReversed.begin(), bwListReversed.end());
        for(auto bw : bwListReversed) {
            if(bw >= samplerate) {
                cur = bw;
            } else {
                break;
            }
        }
        spdlog::info("Bandwidth for samplerate {0} is {1}", samplerate, cur);
        return cur;
    }

    void selectSampleRate(double samplerate) {
        spdlog::info("Setting sample rate to {0}", samplerate);
        if (sampleRates.size() == 0) {
            devId = -1;
            return;
        }
        bool found = false;
        int i = 0;
        for (auto& sr : sampleRates) {
            if (sr == samplerate) {
                srId = i;
                sampleRate = sr;
                found = true;
                core::setInputSampleRate(sampleRate);
                break;
            }
            i++;
        }
        if (!found) {
            // Select default sample rate
            selectSampleRate(sampleRates[0]);
        }
    }

    void selectDevice(std::string name) {
        if (devList.size() == 0) {
            devId = -1;
            return;
        }
        bool found = false;
        int i = 0;
        for (auto& args : devList) {
            if (args["label"] == name) {
                devArgs = args;
                devId = i;
                found = true;
                break;
            }
            i++;
        }
        if (!found) {
            // If device was not found, select default device instead
            selectDevice(devList[0]["label"]);
            return;
        }

        SoapySDR::Device* dev = SoapySDR::Device::make(devArgs);

        antennaList = dev->listAntennas(SOAPY_SDR_RX, channelId);
        txtAntennaList = "";
        for (const std::string & ant : antennaList) {
            txtAntennaList += ant + '\0';
        }

        gainList = dev->listGains(SOAPY_SDR_RX, channelId);
        delete[] uiGains;
        uiGains = new float[gainList.size()];
        gainRanges.clear();
        
        for (auto gain : gainList) {
            gainRanges.push_back(dev->getGainRange(SOAPY_SDR_RX, channelId, gain));
        }
        
        SoapySDR::RangeList bandwidthRange = dev->getBandwidthRange(SOAPY_SDR_RX, channelId);
        
        txtBwList = "";
        bandwidthList.clear();
        bandwidthList.push_back(-1);
        txtBwList += "Auto";
        txtBwList += '\0';
        
        for(auto bwr : bandwidthRange) {
            float bw = bwr.minimum();
            bandwidthList.push_back(bw);
            if (bw > 1.0e3 && bw <= 1.0e6) {
                txtBwList += to_string_with_precision((bw / 1.0e3), 2) + " kHz";
            } else if (bw > 1.0e6) {
                txtBwList += to_string_with_precision((bw / 1.0e6), 2) + " MHz";
            } else {
                txtBwList += to_string_with_precision(bw, 0);
            }
            txtBwList += '\0';
        }

        sampleRates = dev->listSampleRates(SOAPY_SDR_RX, channelId);
        txtSrList = "";
        for (double sr : sampleRates) {
            if (sr > 1.0e3 && sr <= 1.0e6) {
                txtSrList += to_string_with_precision((sr / 1.0e3), 2) + " kHz";
            } else if (sr > 1.0e6) {
                txtSrList += to_string_with_precision((sr / 1.0e6), 2) + " MHz";
            } else {
                txtSrList += to_string_with_precision(sr, 0);
            }
            txtSrList += '\0';
        }

        hasAgc = dev->hasGainMode(SOAPY_SDR_RX, channelId);

        SoapySDR::Device::unmake(dev);

        config.acquire();
        if (config.conf["devices"].contains(name)) {
            if(config.conf["devices"][name].contains("antenna")) {
                uiAntennaId = config.conf["devices"][name]["antenna"];
            } else {
                uiAntennaId = 0;
            }
            int i = 0;
            for (auto gain : gainList) {
                if (config.conf["devices"][name]["gains"].contains(gain)) {
                    uiGains[i] = config.conf["devices"][name]["gains"][gain];
                }
                else {
                    uiGains[i] = gainRanges[i].minimum();
                }
                i++;
            }
            if(config.conf["devices"][name].contains("bandwidth")) {
                uiBandwidthId = config.conf["devices"][name]["bandwidth"];
            } else if(bandwidthList.size() > 2) {
                uiBandwidthId = 0;
            }
            if (hasAgc && config.conf["devices"][name].contains("agc")) {
                agc = config.conf["devices"][name]["agc"];
            }
            else {
                agc = false;
            }
            if (config.conf["devices"][name].contains("sampleRate")) {
                selectSampleRate(config.conf["devices"][name]["sampleRate"]);
            }
            else {
                selectSampleRate(sampleRates[0]);
            }
        }
        else {
            uiAntennaId = 0;
            int i = 0;
            for (auto gain : gainList) {
                uiGains[i] = gainRanges[i].minimum();
                i++;
            }
            if(bandwidthList.size() > 2)
                uiBandwidthId = 0;
            if (hasAgc) {
                agc = false;
            }
            selectSampleRate(sampleRates[0]); // Select default
        }
        config.release();

    }

    void saveCurrent() {
        json conf;
        conf["sampleRate"] = sampleRate;
        conf["antenna"] = uiAntennaId;
        int i = 0;
        for (auto gain : gainList) {
            conf["gains"][gain] = uiGains[i];
            i++;
        }
        if(bandwidthList.size() > 2)
            conf["bandwidth"] = uiBandwidthId;
        if (hasAgc) {
            conf["agc"] = agc;
        }
        config.acquire();
        config.conf["devices"][devArgs["label"]] = conf;
        config.release(true);
    }

    static void menuSelected(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        spdlog::info("SoapyModule '{0}': Menu Select!", _this->name);
        if (_this->devList.size() == 0) {
            return;
        }
        core::setInputSampleRate(_this->sampleRate);
    }

    static void menuDeselected(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        spdlog::info("SoapyModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        if (_this->running) { return; }
        if (_this->devId < 0) {
            spdlog::error("No device available");
            return;
        }

        _this->dev = SoapySDR::Device::make(_this->devArgs);

        _this->dev->setSampleRate(SOAPY_SDR_RX, _this->channelId, _this->sampleRate);

        _this->dev->setAntenna(SOAPY_SDR_RX, _this->channelId, _this->antennaList[_this->uiAntennaId]);

        if(_this->bandwidthList.size() > 2) {
            if(_this->bandwidthList[_this->uiBandwidthId] == -1)
                _this->dev->setBandwidth(SOAPY_SDR_RX, _this->channelId, _this->selectBwBySr(_this->sampleRates[_this->srId]));
            else
                _this->dev->setBandwidth(SOAPY_SDR_RX, _this->channelId, _this->bandwidthList[_this->uiBandwidthId]);
        }

        if (_this->hasAgc) {
            _this->dev->setGainMode(SOAPY_SDR_RX, _this->channelId, _this->agc);
        }

        int i = 0;
        for (auto gain : _this->gainList) {
            _this->dev->setGain(SOAPY_SDR_RX, _this->channelId, gain, _this->uiGains[i]);
            i++;
        }

        _this->dev->setFrequency(SOAPY_SDR_RX, _this->channelId, _this->freq);

        _this->devStream = _this->dev->setupStream(SOAPY_SDR_RX, "CF32");
        _this->dev->activateStream(_this->devStream);
        _this->running = true;
        _this->workerThread = std::thread(_worker, _this);
        spdlog::info("SoapyModule '{0}': Start!", _this->name);
    }
    
    static void stop(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->dev->deactivateStream(_this->devStream);
        _this->dev->closeStream(_this->devStream);
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();
        SoapySDR::Device::unmake(_this->dev);
        
        spdlog::info("SoapyModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            _this->dev->setFrequency(SOAPY_SDR_RX, _this->channelId, freq);
        }
        spdlog::info("SoapyModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        SoapyModule* _this = (SoapyModule*)ctx;
        
        // If no device is available, do not attempt to display menu
        if (_this->devId < 0) {
            return;
        }

        float menuWidth = ImGui::GetContentRegionAvailWidth();

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_dev_select_", _this->name), &_this->devId, _this->txtDevList.c_str())) {
            _this->selectDevice(_this->devList[_this->devId]["label"]);
            config.acquire();
            config.conf["device"] = _this->devList[_this->devId]["label"];
            config.release(true);
        }

        if (ImGui::Combo(CONCAT("##_sr_select_", _this->name), &_this->srId, _this->txtSrList.c_str())) {
            _this->selectSampleRate(_this->sampleRates[_this->srId]);
            if(_this->bandwidthList.size() > 2 && _this->running && _this->bandwidthList[_this->uiBandwidthId] == -1)
                _this->dev->setBandwidth(SOAPY_SDR_RX, _this->channelId, _this->selectBwBySr(_this->sampleRates[_this->srId]));
            _this->saveCurrent();
        }

        ImGui::SameLine();
        float refreshBtnWdith = menuWidth - ImGui::GetCursorPosX();
        if (ImGui::Button(CONCAT("Refresh##_dev_select_", _this->name), ImVec2(refreshBtnWdith, 0))) {
            _this->refresh();
            _this->selectDevice(config.conf["device"]);
        }

        if (_this->running) { style::endDisabled(); }

        if (_this->antennaList.size() > 1) {
            ImGui::Text("Antenna");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::Combo(CONCAT("##_antenna_select_", _this->name), &_this->uiAntennaId, _this->txtAntennaList.c_str())) {
                if (_this->running)
                    _this->dev->setAntenna(SOAPY_SDR_RX, _this->channelId, _this->antennaList[_this->uiAntennaId]);
                _this->saveCurrent();
            }
        }

        float gainNameLen = 0;
        float len;
        for (auto gain : _this->gainList) {
            len = ImGui::CalcTextSize((gain + " gain").c_str()).x;
            if (len > gainNameLen) {
                gainNameLen = len;
            }
        }
        gainNameLen += 5.0f;

        if (_this->hasAgc) {
            if (ImGui::Checkbox((std::string("AGC##_agc_sel_") + _this->name).c_str(), &_this->agc)) {
                if (_this->running) { _this->dev->setGainMode(SOAPY_SDR_RX, _this->channelId, _this->agc); }
                // When disabled, reset the gains
                if (!_this->agc) {
                    int i = 0;
                    for (auto gain : _this->gainList) {
                        _this->dev->setGain(SOAPY_SDR_RX, _this->channelId, gain, _this->uiGains[i]);
                        i++;
                    }
                }
                _this->saveCurrent();
            }
        }

        int i = 0;
        for (auto gain : _this->gainList) {
            ImGui::Text("%s gain", gain.c_str());
            ImGui::SameLine();
            ImGui::SetCursorPosX(gainNameLen);
            ImGui::SetNextItemWidth(menuWidth - gainNameLen);
            float step = _this->gainRanges[i].step();
            bool res;
            if(step == 0.0f) {
                res = ImGui::SliderFloat((std::string("##_gain_sel_") + _this->name  + gain).c_str(), &_this->uiGains[i], _this->gainRanges[i].minimum(), _this->gainRanges[i].maximum());
            } else {
                res = ImGui::SliderFloatWithSteps((std::string("##_gain_sel_") + _this->name + gain).c_str(), &_this->uiGains[i], _this->gainRanges[i].minimum(), _this->gainRanges[i].maximum(), step);
            }
            if(res) {
                if (_this->running) {
                    _this->dev->setGain(SOAPY_SDR_RX, _this->channelId, gain, _this->uiGains[i]);
                }
                _this->saveCurrent();
            }
            i++;
        }
        if(_this->bandwidthList.size() > 2) {
            float bwLen = ImGui::CalcTextSize("Bandwidth").x + 5.0f;
            ImGui::Text("Bandwidth");
            ImGui::SameLine();
            ImGui::SetCursorPosX(bwLen);
            ImGui::SetNextItemWidth(menuWidth - bwLen);
        
            if (ImGui::Combo(CONCAT("##_bw_select_", _this->name), &_this->uiBandwidthId, _this->txtBwList.c_str())) {
                if(_this->running) {
                    if(_this->bandwidthList[_this->uiBandwidthId] == -1)
                        _this->dev->setBandwidth(SOAPY_SDR_RX, _this->channelId, _this->selectBwBySr(_this->sampleRates[_this->srId]));
                    else
                        _this->dev->setBandwidth(SOAPY_SDR_RX, _this->channelId, _this->bandwidthList[_this->uiBandwidthId]);
                }
                _this->saveCurrent();
            }
        }
    }

    static void _worker(SoapyModule* _this) {
        int blockSize = _this->sampleRate / 200.0f;
        int flags = 0;
        long long timeMs = 0;
        
        while (_this->running) {
            int res = _this->dev->readStream(_this->devStream, (void**)&_this->stream.writeBuf, blockSize, flags, timeMs);
            if (res < 1) {
                continue;
            }
            if (!_this->stream.swap(res)) { return; }
        }
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    SoapySDR::Stream* devStream;
    SourceManager::SourceHandler handler;
    SoapySDR::KwargsList devList;
    SoapySDR::Kwargs devArgs;
    SoapySDR::Device* dev;
    std::string txtDevList;
    std::string txtSrList;
    std::thread workerThread;
    int devId = -1;
    double freq;
    double sampleRate;
    bool running = false;
    bool hasAgc = false;
    bool agc = false;
    std::vector<double> sampleRates;
    int srId = -1;
    float* uiGains;
    int channelCount = 1;
    int channelId = 0;
    int uiAntennaId = 0;
    std::vector<std::string> antennaList;
    std::string txtAntennaList;
    std::vector<std::string> gainList;
    std::vector<SoapySDR::Range> gainRanges;
    int uiBandwidthId = 0;
    std::vector<float> bandwidthList;
    std::string txtBwList;
};

MOD_EXPORT void _INIT_() {
   config.setPath(options::opts.root + "/soapy_source_config.json");
   json defConf;
   defConf["device"] = "";
   defConf["devices"] = json({});
   config.load(defConf);
   config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SoapyModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (SoapyModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
