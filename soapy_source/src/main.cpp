#include <imgui.h>
#include <spdlog/spdlog.h>
#include <new_module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Logger.hpp>
#include <core.h>
#include <gui/style.h>

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
        config.aquire();
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

        spdlog::info("SoapyModule '{0}': Instance created!", name);
    }

    ~SoapyModule() {
        spdlog::info("SoapyModule '{0}': Instance deleted!", name);
    }

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
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

        gainList = dev->listGains(SOAPY_SDR_RX, channelId);
        delete[] uiGains;
        uiGains = new float[gainList.size()];
        for (auto gain : gainList) {
            gainRanges.push_back(dev->getGainRange(SOAPY_SDR_RX, channelId, gain));
        }

        sampleRates = dev->listSampleRates(SOAPY_SDR_RX, channelId);
        txtSrList = "";
        for (double sr : sampleRates) {
            if (sr > 1.0e3 && sr <= 1.0e6) {

                txtSrList += std::to_string((sr / 1.0e3)) + " kHz";
            } else if (sr > 1.0e6) {
                txtSrList += std::to_string((sr / 1.0e6)) + " MHz";
            } else {
                txtSrList += std::to_string((int) sr);
            }
            txtSrList += '\0';
        }

        hasAgc = dev->hasGainMode(SOAPY_SDR_RX, channelId);

        SoapySDR::Device::unmake(dev);

        config.aquire();
        if (config.conf["devices"].contains(name)) {
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
            int i = 0;
            for (auto gain : gainList) {
                uiGains[i] = gainRanges[i].minimum();
                i++;
            }
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
        int i = 0;
        for (auto gain : gainList) {
            conf["gains"][gain] = uiGains[i];
            i++;
        }
        if (hasAgc) {
            conf["agc"] = agc;
        }
        config.aquire();
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
        _this->dev = SoapySDR::Device::make(_this->devArgs);

        _this->dev->setSampleRate(SOAPY_SDR_RX, _this->channelId, _this->sampleRate);

        int i = 0;
        for (auto gain : _this->gainList) {
            _this->dev->setGain(SOAPY_SDR_RX, _this->channelId, gain, _this->uiGains[i]);
            i++;
        }

        if (_this->hasAgc) {
            _this->dev->setGainMode(SOAPY_SDR_RX, _this->channelId,  _this->agc);
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
            config.aquire();
            config.conf["device"] = _this->devList[_this->devId]["label"];
            config.release(true);
        }

        ImGui::Text("Sample rate");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_sr_select_", _this->name), &_this->srId, _this->txtSrList.c_str())) {
            _this->selectSampleRate(_this->sampleRates[_this->srId]);
            _this->saveCurrent();
        }
        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Button(CONCAT("Refresh##_dev_select_", _this->name))) {
            _this->refresh();
            _this->selectDevice(config.conf["device"]);
        }

        if (_this->running) { style::endDisabled(); }

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
            if (ImGui::SliderFloat((gain + std::string("##_gain_sel_") + _this->name).c_str(), &_this->uiGains[i], 
                                _this->gainRanges[i].minimum(), _this->gainRanges[i].maximum())) {
                if (_this->running) {
                    _this->dev->setGain(SOAPY_SDR_RX, _this->channelId, gain, _this->uiGains[i]);
                }
                _this->saveCurrent();
            }
            i++;
        }
    }

    static void _worker(SoapyModule* _this) {
        int blockSize = _this->sampleRate / 200.0f;
        int flags = 0;
        long long timeMs = 0;
        
        while (_this->running) {
            if (_this->stream.aquire() < 0) { break; }
            int res = _this->dev->readStream(_this->devStream, (void**)&_this->stream.data, blockSize, flags, timeMs);
            if (res < 1) {
                continue;
            }
            _this->stream.write(res);
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
    std::vector<std::string> gainList;
    std::vector<SoapySDR::Range> gainRanges;
};

MOD_EXPORT void _INIT_() {
   config.setPath(ROOT_DIR "/soapy_source_config.json");
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