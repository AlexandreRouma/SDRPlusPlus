#include "device.h"
#include "devices.h"
#include "uhd_device.h"

#include "config.h"
#include "core.h"
#include "dsp/stream.h"
#include "dsp/types.h"
#include "gui/style.h"
#include "json.hpp"
#include "module.h"
#include "options.h"
#include "signal_path/source.h"
#include "signal_path/signal_path.h"

#include "spdlog/spdlog.h"

#include <string>
#include <memory>
#include <vector>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "uhd_source",
    /* Description:     */ "UHD source module for SDR++",
    /* Author:          */ "",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

std::vector<double> sampleRates;
std::vector<double> bandwidths;

namespace {
    //! Conditionally append find_all=1 if the key isn't there yet
    uhd::device_addr_t append_findall(const uhd::device_addr_t& device_args) {
        uhd::device_addr_t new_device_args(device_args);
        if (!new_device_args.has_key("find_all")) {
            new_device_args["find_all"] = "1";
        }

        return new_device_args;
    }
} // namespace

class UHDSourceModule : public ModuleManager::Instance {
public:
    UHDSourceModule(std::string name) : name(std::move(name)) {
        sourceHandler.ctx = this;
        sourceHandler.selectHandler = &UHDSourceModule::selectHandler;
        sourceHandler.deselectHandler = &UHDSourceModule::deselectHandler;
        sourceHandler.menuHandler = &UHDSourceModule::menuHandler;
        sourceHandler.startHandler = &UHDSourceModule::startHandler;
        sourceHandler.stopHandler = &UHDSourceModule::stopHandler;
        sourceHandler.tuneHandler = &UHDSourceModule::tuneHandler;
        sourceHandler.stream = &stream;

        findUHDDevices();

        config.acquire();
        const std::string lastUsedDevice = config.conf[DEVICE_FIELD];
        config.release();

        const Device& device = devices.getDeviceBySerial(lastUsedDevice);
        if (device.isValid()) {
            spdlog::debug("device {0} is valid", device.serial());
            deviceIndex = devices.getDeviceIndexBySerial(device.serial());
            openUHDDevice(this);
        }
        else {
            // TODO: is executed but does not write empty string which acquire/release
            spdlog::debug("device {0} is not valid", device.serial());
            config.conf[DEVICE_FIELD] = "";
            config.save();
        }

        sigpath::sourceManager.registerSource("UHD", &sourceHandler);
    }

    ~UHDSourceModule() {
        sigpath::sourceManager.unregisterSource("UHD");
    }

    void postInit() {
        spdlog::debug("UHDSourceModule::postInit");
    }

    void enable() {
        spdlog::debug("UHDSourceModule::enable");
        enabled = true;
    }

    void disable() {
        spdlog::debug("UHDSourceModule::disable");
        enabled = false;
    }

    bool isEnabled() {
        spdlog::debug("UHDSourceModule::isEnabled");
        return enabled;
    }

    static const std::string DEVICE_FIELD;
    static const std::string DEVICES_FIELD;

private:
    // SourceHandler
    static void menuHandler(void* ctx) {
        UHDSourceModule* _this = (UHDSourceModule*)ctx;
        const float menuWidth = ImGui::GetContentRegionAvailWidth();
        const std::string concatenatedDeviceList = _this->getDeviceListString();

        if (_this->receiving) { style::beginDisabled(); }

        const std::string deviceName(_this->devices.at(_this->deviceIndex).serial());

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_uhd_dev_sel_", deviceName), &_this->deviceIndex, concatenatedDeviceList.c_str())) {
            spdlog::debug("selected device index is {0}", _this->deviceIndex);
            openUHDDevice(ctx);
            config.acquire();
            config.conf[DEVICE_FIELD] = _this->devices.at(_this->deviceIndex).serial();
            config.release(true);
        }

        if (ImGui::Button(CONCAT("Refresh##uhd_refresh_", deviceName), ImVec2(menuWidth, 0))) {
            _this->findUHDDevices();
            // if new devices are found, the order could change
            _this->deviceIndex = _this->devices.getDeviceIndexBySerial(deviceName);
        }

        const bool validDeviceOpen = _this->uhdDevice && _this->uhdDevice->isOpen();

        if (_this->receiving) { style::endDisabled(); }

        if (!validDeviceOpen) {
            _this->sampleRateIndex = 0;
            _this->bandwidthIndex = 0;
            _this->rxGain = 0;
            _this->rxAntennaIndex = 0;
            _this->rxChannelIndex = 0;
            sampleRates.clear();
            bandwidths.clear();
            style::beginDisabled();
        }

        ImGui::LeftLabel("Sample rate");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##uhd_sample_rate_", deviceName), &_this->sampleRateIndex, _this->getRxSampleRateListString().c_str())) {
            if (validDeviceOpen) {
                _this->uhdDevice->setRxSampleRate(sampleRates[_this->sampleRateIndex]);
                core::setInputSampleRate(_this->uhdDevice->getRxSampleRate());
                config.acquire();
                config.conf[DEVICES_FIELD][_this->uhdDevice->serial()][SAMPLE_RATE_INDEX_FIELD] = _this->sampleRateIndex;
                config.release(true);
            }
        }

        ImGui::LeftLabel("Bandwidth");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##uhd_bandwidth_", deviceName), &_this->bandwidthIndex, _this->getRxBandwidthListString().c_str())) {
            if (validDeviceOpen) {
                _this->uhdDevice->setRxBandwidth(bandwidths[_this->bandwidthIndex]);
                config.acquire();
                config.conf[DEVICES_FIELD][_this->uhdDevice->serial()][BANDWIDTH_INDEX_FIELD] = _this->bandwidthIndex;
                config.release(true);
            }
        }

        int rxGainMin = _this->uhdDevice ? _this->uhdDevice->getRxGainMin() : 0;
        int rxGainMax = _this->uhdDevice ? _this->uhdDevice->getRxGainMax() : 0;
        std::string gainLabel{ "Rx Gain" };
        ImGui::PushItemWidth(menuWidth - ImGui::CalcTextSize(gainLabel.c_str()).x - 10);
        ImGui::LeftLabel(gainLabel.c_str());
        if (ImGui::SliderInt(CONCAT("##uhd_rx_gain_", deviceName), &_this->rxGain, rxGainMin, rxGainMax, "%d dB")) {
            if (validDeviceOpen) {
                _this->uhdDevice->setRxGain(_this->rxGain);
                config.acquire();
                config.conf[DEVICES_FIELD][_this->uhdDevice->serial()][RX_GAIN_FIELD] = _this->rxGain;
                config.release(true);
            }
        }
        ImGui::LeftLabel("Antenna");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##uhd_antenna_", deviceName), &_this->rxAntennaIndex, _this->getRxAntennaListString().c_str())) {
            if (validDeviceOpen) {
                _this->uhdDevice->setRxAntennaByIndex(_this->rxAntennaIndex);
                config.acquire();
                config.conf[DEVICES_FIELD][_this->uhdDevice->serial()][ANTENNA_INDEX_FIELD] = _this->rxAntennaIndex;
                config.release(true);
            }
        }
        ImGui::LeftLabel("Channel");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##uhd_chan_", deviceName), &_this->rxChannelIndex, _this->getRxChannelListString().c_str())) {
            if (validDeviceOpen) {
                _this->uhdDevice->setChannelIndex(_this->rxChannelIndex);
                config.acquire();
                config.conf[DEVICES_FIELD][_this->uhdDevice->serial()][CHANNEL_INDEX_FIELD] = _this->rxChannelIndex;
                config.release(true);
            }
        }

        if (!validDeviceOpen) { style::endDisabled(); }
    }

    static void selectHandler(void* ctx) {
        spdlog::debug("UHDSourceModule::selectHandler");
        UHDSourceModule* _this = (UHDSourceModule*)ctx;
    }

    static void deselectHandler(void* ctx) {
        spdlog::debug("UHDSourceModule::deselectHandler");
        UHDSourceModule* _this = (UHDSourceModule*)ctx;
    }

    static void startHandler(void* ctx) {
        spdlog::debug("UHDSourceModule::startHandler");
        UHDSourceModule* _this = (UHDSourceModule*)ctx;

        if (_this->uhdDevice) {
            _this->uhdDevice->startReceiving(_this->stream);
            _this->receiving = true;
        }
    }

    static void stopHandler(void* ctx) {
        spdlog::debug("UHDSourceModule::stopHandler");
        UHDSourceModule* _this = (UHDSourceModule*)ctx;
        if (_this->uhdDevice) {
            spdlog::debug("stop receiving data");
            _this->uhdDevice->stopReceiving();
        }
        _this->receiving = false;
    }

    static void tuneHandler(double freq, void* ctx) {
        spdlog::debug("UHDSourceModule::tuneHandler: freq = {0}", freq);
        UHDSourceModule* _this = (UHDSourceModule*)ctx;
        if (_this->uhdDevice) {
            _this->uhdDevice->setCenterFrequency(freq);
        }
    }

    // helpers
    static void openUHDDevice(void* ctx) {
        UHDSourceModule* _this = (UHDSourceModule*)ctx;
        if (_this->devices.empty() || (_this->deviceIndex < 1)) {
            spdlog::info("select a valid device, not doing anything");
            _this->uhdDevice = nullptr;
            return;
        }

        _this->uhdDevice = std::make_unique<UHDDevice>(_this->devices.at(_this->deviceIndex));
        _this->uhdDevice->open();
        if (!_this->uhdDevice->isOpen()) {
            spdlog::error("could not open device with serial {0}", _this->devices.at(_this->deviceIndex).serial());
            _this->uhdDevice = nullptr;
            return;
        }

        _this->fillSampleRates(_this->uhdDevice->getRxSampleRateMax());
        _this->fillBandwidths(_this->uhdDevice->getRxBandwidthMax());

        // if we have settings for the device stored, load and apply them
        config.acquire();
        const std::string currentDevice = _this->uhdDevice->serial();
        if (config.conf[DEVICES_FIELD].contains(currentDevice)) {
            nlohmann::json deviceConfig = config.conf[DEVICES_FIELD][currentDevice];
            if (deviceConfig.contains(ANTENNA_INDEX_FIELD)) {
                _this->rxAntennaIndex = deviceConfig[ANTENNA_INDEX_FIELD];
            }
            if (deviceConfig.contains(RX_GAIN_FIELD)) {
                _this->rxGain = deviceConfig[RX_GAIN_FIELD];
            }
            if (deviceConfig.contains(SAMPLE_RATE_INDEX_FIELD)) {
                _this->sampleRateIndex = deviceConfig[SAMPLE_RATE_INDEX_FIELD];
            }
            if (deviceConfig.contains(BANDWIDTH_INDEX_FIELD)) {
                _this->bandwidthIndex = deviceConfig[BANDWIDTH_INDEX_FIELD];
            }
        }
        config.release();

        _this->uhdDevice->setChannelIndex(_this->rxChannelIndex);
        _this->uhdDevice->setRxGain(_this->rxGain);
        _this->uhdDevice->setRxAntennaByIndex(_this->rxAntennaIndex);
        _this->uhdDevice->setRxSampleRate(sampleRates[_this->sampleRateIndex]);
        core::setInputSampleRate(_this->uhdDevice->getRxSampleRate());
        spdlog::debug("devie opened: index = {0}, serial = {1}", _this->deviceIndex, _this->devices.at(_this->deviceIndex).serial());
    }

    void findUHDDevices() {
        spdlog::debug("UHDSourceModule::findUHDDevices");

        devices.clear();
        const uhd::device_addr_t args("");
        uhd::device_addrs_t device_addrs = uhd::device::find(append_findall(args));
        if (device_addrs.empty()) {
            spdlog::info("No UHD Devices Found");
        }

        for (const auto& device_addr : device_addrs) {
            const int index = devices.add(Device{});
            Device& device = devices.at(index);
            device.name(device_addr[Device::NAME_FIELD]);
            device.product(device_addr[Device::PRODUCT_FIELD]);
            device.serial(device_addr[Device::SERIAL_FIELD]);
            device.type(device_addr[Device::TYPE_FIELD]);
            spdlog::debug("Device");
            spdlog::debug("    Serial: {0}", device.serial());
            spdlog::debug("    Product: {0}", device.product());
            spdlog::debug("    Name: {0}", device.name());
            spdlog::debug("    Type: {0}", device.type());
        }

        if (!devices.empty()) {
            devices.sortBySerial();
            spdlog::info("found {0} device(s)", devices.size());
        }
    }

    std::string getDeviceListString() const {
        std::stringstream deviceList;
        for (const auto& device : devices) {
            if (device.serial().empty()) {
                deviceList << device.product();
            }
            else {
                deviceList << device.product() << " (" << device.serial() << ")";
            }
            deviceList.write("\0", 1);
        }
        return deviceList.str();
    }

    std::string getRxChannelListString() const {
        std::stringstream channelList;
        if (uhdDevice) {
            for (size_t channelIndex = 0; channelIndex < uhdDevice->getRxChannelCount(); ++channelIndex) {
                channelList << channelIndex;
                channelList.write("\0", 1);
            }
        }
        return channelList.str();
    }

    std::string getRxAntennaListString() const {
        std::stringstream antennaList;
        if (uhdDevice) {
            for (const auto& antenna : uhdDevice->getAntennas()) {
                antennaList << antenna;
                antennaList.write("\0", 1);
            }
        }
        return antennaList.str();
    }

    std::string getRxSampleRateListString() const {
        std::stringstream sampleRateList;
        for (const auto sampleRate : sampleRates) {
            sampleRateList << sampleRate / 1000000 << " MHz";
            sampleRateList.write("\0", 1);
        }
        return sampleRateList.str();
    }

    std::string getRxBandwidthListString() const {
        std::stringstream bandwidthList;
        for (const auto sampleRate : bandwidths) {
            bandwidthList << sampleRate / 1000000 << " MHz";
            bandwidthList.write("\0", 1);
        }
        return bandwidthList.str();
    }

    void fillValueRange(std::vector<double>& container, double from, double to, double increment) {
        container.clear();
        for (double i = from; i <= to; i += increment) {
            container.push_back(i);
        }
    }

    void fillSampleRates(double to) {
        fillValueRange(sampleRates, 1000000, to, 1000000);
    }

    void fillBandwidths(double to) {
        fillValueRange(bandwidths, 1000000, to, 1000000);
    }

    bool enabled = false;
    bool receiving = false;
    int deviceIndex = 0; // currently selected device index
    int sampleRateIndex = 0;
    int bandwidthIndex = 0;
    int rxGain = 0;
    int rxChannelIndex = 0;
    int rxAntennaIndex = 0;
    std::string name;
    std::unique_ptr<UHDDevice> uhdDevice;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler sourceHandler;
    Devices devices;

    static const std::string ANTENNA_INDEX_FIELD;
    static const std::string RX_GAIN_FIELD;
    static const std::string SAMPLE_RATE_INDEX_FIELD;
    static const std::string BANDWIDTH_INDEX_FIELD;
    static const std::string CHANNEL_INDEX_FIELD;
};

const std::string UHDSourceModule::DEVICE_FIELD = "device";
const std::string UHDSourceModule::DEVICES_FIELD = "devices";
const std::string UHDSourceModule::ANTENNA_INDEX_FIELD = "antennaIndex";
const std::string UHDSourceModule::RX_GAIN_FIELD = "rxGain";
const std::string UHDSourceModule::SAMPLE_RATE_INDEX_FIELD = "sampleRateIndex";
const std::string UHDSourceModule::BANDWIDTH_INDEX_FIELD = "bandwidthIndex";
const std::string UHDSourceModule::CHANNEL_INDEX_FIELD = "channel";

MOD_EXPORT void _INIT_() {
    json def = json({});
    def[UHDSourceModule::DEVICE_FIELD] = "";
    def[UHDSourceModule::DEVICES_FIELD] = json({});
    config.setPath(options::opts.root + "/uhd_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new UHDSourceModule(std::move(name));
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (UHDSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}