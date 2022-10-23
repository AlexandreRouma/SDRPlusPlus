#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <gui/widgets/stepped_slider.h>
#include <uhd.h>
#include <uhd/device.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <utils/optionlist.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "usrp_source",
    /* Description:     */ "USRP source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class USRPSourceModule : public ModuleManager::Instance {
public:
    USRPSourceModule(std::string name) {
        this->name = name;

        sampleRate = 8000000.0;
        // TODO: REMOVE
        samplerates.define(8000000, "8MHz", 8000000.0);

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        // List devices
        refresh();

        // Select device
        config.acquire();
        selectedSer = config.conf["device"];
        config.release();
        select(selectedSer);

        sigpath::sourceManager.registerSource("USRP", &handler);
    }

    ~USRPSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("USRP");
    }

    void postInit() {}

    enum AGCMode {
        AGC_MODE_OFF,
        AGC_MODE_LOW,
        AGC_MODE_HIGG
    };

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

    void refresh() {
        devices.clear();
        uhd::device_addr_t hint;
        uhd::device_addrs_t devList = uhd::device::find(hint);

        char buf[1024];
        for (const auto& devAddr : devList) {
            std::string serial = devAddr["serial"];
            std::string model = devAddr.has_key("product") ? devAddr["product"] : devAddr["type"];
            sprintf(buf, "USRP %s [%s]", model.c_str(), serial.c_str());
            devices.define(serial, buf, devAddr);
        }
    }

    void select(std::string serial) {
        // If no device, give up
        if (!devices.size()) {
            selectedSer.clear();
            return;
        }

        // If the wanted serial is not available, select first
        if (!devices.keyExists(serial)) {
            select(devices.key(0));
            return;
        }

        // Update selection
        selectedSer = serial;
        devId = devices.keyId(serial);

        // Make device
        auto dev = uhd::usrp::multi_usrp::make(devices[devId]);

        // List subdevices
        char buf[1024];
        channels.clear();
        auto subdevs = dev->get_rx_subdev_spec();
        for (int i = 0; i < subdevs.size(); i++) {
            std::string slot = subdevs[i].db_name;
            sprintf(buf, "%s [%s]", dev->get_rx_subdev_name(i).c_str(), slot.c_str());
            channels.define(buf, buf, slot);
        }

        // Select channel
        std::string chan = "";
        config.acquire();
        if (config.conf["devices"][selectedSer].contains("channel")) {
            chan = config.conf["devices"][selectedSer]["channel"];
        }
        config.release();
        selectChannel(dev, chan);
    }

    void selectChannel(uhd::usrp::multi_usrp::sptr dev, std::string chan) {
        // If wanted channel is not available, select first
        if (!channels.keyExists(chan)) {
            selectChannel(dev, channels.key(0));
            return;
        }

        // Update selection
        selectedChan = chan;
        chanId = channels.keyId(chan);

        // List samplerates
        samplerates.clear();
        auto srList = dev->get_rx_rates(chanId);
        for (auto& l : srList) {
            if (l.step() == 0.0 || l.start() == l.stop()) {
                samplerates.define(l.start(), getBandwdithScaled(l.start()), l.start());
            }
            else {
                for (double f = l.start(); f <= l.stop(); f += l.step()) {
                    samplerates.define(f, getBandwdithScaled(f), f);
                }
            }
        }

        // List antennas
        antennas.clear();
        auto ants = dev->get_rx_antennas(chanId);
        for (const auto& a : ants) {
            antennas.define(a,a,a);
        }

        // Get gain range
        gainRange = dev->get_rx_gain_range(chanId)[0];

        // Load settings
        srId = 0;
        antId = 0;
        gain = gainRange.start();
        config.acquire();
        if (config.conf["devices"][selectedSer].contains("channels") && config.conf["devices"][selectedSer]["channels"].contains(selectedChan)) {
            auto cconf = config.conf["devices"][selectedSer]["channels"][selectedChan];
            if (cconf.contains("samplerate")) {
                int sr = cconf["samplerate"];
                if (samplerates.keyExists(sr)) { srId = samplerates.keyId(sr); }
            }
            if (cconf.contains("antenna")) {
                std::string ant = cconf["antenna"];
                if (antennas.keyExists(ant)) { antId = antennas.keyId(ant); }
            }
            if (cconf.contains("gain")) {
                gain = cconf["gain"];
                gain = std::clamp<float>(gain, gainRange.start(), gainRange.stop());
            }
        }
        config.release();

        // Apply samplerate
        sampleRate = samplerates.key(srId);
        core::setInputSampleRate(sampleRate);
    }

private:
    std::string getBandwdithScaled(double bw) {
        char buf[1024];
        // if (bw >= 1000000.0) {
        //     sprintf(buf, "%.1lfMHz", bw / 1000000.0);
        // }
        // else if (bw >= 1000.0) {
        //     sprintf(buf, "%.1lfKHz", bw / 1000.0);
        // }
        // else {
            sprintf(buf, "%.1lfHz", bw);
        //}
        return std::string(buf);
    }

    static void menuSelected(void* ctx) {
        USRPSourceModule* _this = (USRPSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("USRPSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        USRPSourceModule* _this = (USRPSourceModule*)ctx;
        spdlog::info("USRPSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        USRPSourceModule* _this = (USRPSourceModule*)ctx;
        if (_this->running) { return; }
        if (_this->selectedSer.empty()) { return; }

        _this->dev = uhd::usrp::multi_usrp::make(_this->devices[_this->devId]);

        _this->dev->set_rx_rate(_this->sampleRate, _this->chanId);
        _this->dev->set_rx_antenna(_this->antennas.key(_this->antId), _this->chanId);
        _this->dev->set_rx_gain(_this->gain, _this->chanId);
        _this->dev->set_rx_freq(_this->freq, _this->chanId);
        
        uhd::stream_args_t sargs;
        sargs.channels.clear();
        sargs.channels.push_back(_this->chanId);
        sargs.cpu_format = "fc32";
        sargs.otw_format = "sc16";
        _this->streamer = _this->dev->get_rx_stream(sargs);
        _this->streamer->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
        
        _this->stream.clearWriteStop();
        _this->workerThread = std::thread(&USRPSourceModule::worker, _this);

        _this->running = true;
        spdlog::info("USRPSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        USRPSourceModule* _this = (USRPSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        _this->stream.stopWriter();
        _this->streamer->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
        if (_this->workerThread.joinable()) { _this->workerThread.join(); }
        _this->stream.clearWriteStop();
        
        _this->streamer.reset();
        _this->dev.reset();

        spdlog::info("USRPSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        USRPSourceModule* _this = (USRPSourceModule*)ctx;
        if (_this->running) {
            _this->dev->set_rx_freq(freq, _this->chanId);
        }
        _this->freq = freq;
        spdlog::info("USRPSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        USRPSourceModule* _this = (USRPSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_usrp_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->devId));
            if (!_this->selectedSer.empty()) {
                config.acquire();
                config.conf["device"] = _this->devices.key(_this->devId);
                config.release(true);
            }
        }

        if (SmGui::Combo(CONCAT("##_usrp_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates.key(_this->srId);
            core::setInputSampleRate(_this->sampleRate);
            if (!_this->selectedSer.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSer]["channels"][_this->selectedChan]["samplerate"] = _this->samplerates.key(_this->srId);
                config.release(true);
            }
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_usrp_refr_", _this->name))) {
            _this->refresh();
            config.acquire();
            std::string ser = config.conf["device"];
            config.release();
            _this->select(ser);
        }

        if (_this->channels.size() > 1) {
            SmGui::LeftLabel("Channel");
            SmGui::FillWidth();
            SmGui::ForceSync();
            if (SmGui::Combo(CONCAT("##_usrp_ch_sel_", _this->name), &_this->chanId, _this->channels.txt)) {
                if (!_this->selectedSer.empty()) {
                    config.acquire();
                    config.conf["devices"][_this->selectedSer]["channel"] = _this->channels.key(_this->chanId);
                    config.release(true);
                }
                _this->select(_this->devices.key(_this->devId));
            }
        }

        if (_this->running) { SmGui::EndDisabled(); }

        if (_this->antennas.size() > 1) {
            SmGui::LeftLabel("Antenna");
            SmGui::FillWidth();
            if (SmGui::Combo(CONCAT("##_usrp_ant_sel_", _this->name), &_this->antId, _this->antennas.txt)) {
                if (_this->running) {
                    _this->dev->set_rx_antenna(_this->antennas.key(_this->antId), _this->chanId);
                }
                if (!_this->selectedSer.empty() && !_this->selectedChan.empty()) {
                    config.acquire();
                    config.conf["devices"][_this->selectedSer]["channels"][_this->selectedChan]["antenna"] = _this->antennas.key(_this->antId);
                    config.release(true);
                }
            }
        }

        SmGui::LeftLabel("Gain");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_usrp_gain_", _this->name), &_this->gain, _this->gainRange.start(), _this->gainRange.stop(), _this->gainRange.step(), SmGui::FMT_STR_FLOAT_DB_ONE_DECIMAL)) {
            if (_this->running) {
                _this->dev->set_rx_gain(_this->gain, _this->chanId);
            }
            if (!_this->selectedSer.empty() && !_this->selectedChan.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSer]["channels"][_this->selectedChan]["gain"] = _this->gain;
                config.release(true);
            }
        }
    }

    uint32_t floor2(uint32_t val) {
        val |= val >> 1;
        val |= val >> 2;
        val |= val >> 4;
        val |= val >> 8;
        val |= val >> 16;
        return val - (val >> 1);
    }

    void worker() {
        // TODO: Select a better buffer size that will avoid bad timing
        int bufferSize = sampleRate / 200;
        while (true) {
            uhd::rx_metadata_t meta;
            void* ptr[] = { stream.writeBuf };
            uhd::rx_streamer::buffs_type buffers(ptr, 1);
            int len = streamer->recv(stream.writeBuf, bufferSize, meta, 1.0);
            if (len < 0) { break; }
            if (len != bufferSize) {
                printf("%d\n", len);
            }
            if (len) {
                if (!stream.swap(len)) { break; }
            }
        }
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    int devId = 0;
    int chanId = 0;
    int srId = 0;
    int antId = 0;
    std::string selectedSer = "";
    std::string selectedChan = "";
    float gain = 0.0f;

    OptionList<std::string, uhd::device_addr_t> devices;
    OptionList<std::string, std::string> channels;
    OptionList<int, double> samplerates;
    OptionList<std::string, std::string> antennas;
    uhd::range_t gainRange;

    uhd::usrp::multi_usrp::sptr dev;
    uhd::rx_streamer::sptr streamer;

    std::thread workerThread;

};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/usrp_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new USRPSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (USRPSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}