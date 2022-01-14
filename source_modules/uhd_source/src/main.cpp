#include "device.h"
#include "devices.h"
#include "uhddevice.h"

#include "config.h"
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

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "uhd_source",
    /* Description:     */ "UHD source module for SDR++",
    /* Author:          */ "",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager g_config;

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
    UHDSourceModule(std::string name);
    ~UHDSourceModule();

    void postInit();
    void enable();
    void disable();
    bool isEnabled();

private:
    // SourceHandler
    static void menuHandler(void* ctx);
    static void selectHandler(void* ctx);
    static void deselectHandler(void* ctx);
    static void startHandler(void* ctx);
    static void stopHandler(void* ctx);
    static void tuneHandler(double freq, void* ctx);

    // helpers
    static void openUHDDevice(void* ctx);
    void findUHDDevices();
    std::string getDeviceListString() const;

    bool mEnabled;
    bool mReceiving;
    int mDeviceId; // currently selected device index
    int mBandwidth;
    int mGain;
    std::string mName;
    std::unique_ptr<UHDDevice> mpUhdDevice;
    dsp::stream<dsp::complex_t> mStream;
    SourceManager::SourceHandler mSourceHandler;
    Devices mDevices;
    std::string mDeviceArguments;
};

UHDSourceModule::UHDSourceModule(std::string name)
    : mEnabled(false), mReceiving(false), mDeviceId(0), mBandwidth(0), mGain(0), mName(std::move(name)) {
    mSourceHandler.ctx = this;
    mSourceHandler.selectHandler = &UHDSourceModule::selectHandler;
    mSourceHandler.deselectHandler = &UHDSourceModule::deselectHandler;
    mSourceHandler.menuHandler = &UHDSourceModule::menuHandler;
    mSourceHandler.startHandler = &UHDSourceModule::startHandler;
    mSourceHandler.stopHandler = &UHDSourceModule::stopHandler;
    mSourceHandler.tuneHandler = &UHDSourceModule::tuneHandler;
    mSourceHandler.stream = &mStream;

    g_config.acquire();
    // access config
    g_config.release();

    findUHDDevices();

    sigpath::sourceManager.registerSource("UHD", &mSourceHandler);
}

UHDSourceModule::~UHDSourceModule() {
    // TODO: check what needs to be done
    sigpath::sourceManager.unregisterSource("UHD");
}

void UHDSourceModule::postInit() {
    spdlog::debug("UHDSourceModule::postInit");
}

void UHDSourceModule::enable() {
    spdlog::debug("UHDSourceModule::enable");
    mEnabled = true;
}

void UHDSourceModule::disable() {
    spdlog::debug("UHDSourceModule::disable");
}

bool UHDSourceModule::isEnabled() {
    spdlog::debug("UHDSourceModule::isEnabled");
    return mEnabled;
}

void UHDSourceModule::menuHandler(void* ctx) {
    UHDSourceModule* _this = (UHDSourceModule*)ctx;
    const float menuWidth = ImGui::GetContentRegionAvailWidth();
    const std::string concatenatedDeviceList = _this->getDeviceListString();

    if (_this->mReceiving) { style::beginDisabled(); }

    const std::string deviceName(_this->mDevices.at(_this->mDeviceId).serial());

    ImGui::SetNextItemWidth(menuWidth);
    if (ImGui::Combo(CONCAT("##_uhd_dev_sel_", deviceName), &_this->mDeviceId, concatenatedDeviceList.c_str())) {
        spdlog::debug("selected device index is {0}", _this->mDeviceId);
        openUHDDevice(ctx);
        g_config.acquire();
        g_config.conf["device"] = deviceName;
        g_config.release(true);
    }

    if (_this->mReceiving) { style::endDisabled(); }
}

void UHDSourceModule::selectHandler(void* ctx) {
    spdlog::debug("UHDSourceModule::selectHandler");
    UHDSourceModule* _this = (UHDSourceModule*)ctx;
}

void UHDSourceModule::deselectHandler(void* ctx) {
    spdlog::debug("UHDSourceModule::deselectHandler");
    UHDSourceModule* _this = (UHDSourceModule*)ctx;
}

void UHDSourceModule::startHandler(void* ctx) {
    spdlog::debug("UHDSourceModule::startHandler");
    UHDSourceModule* _this = (UHDSourceModule*)ctx;

    if (_this->mpUhdDevice) {
        _this->mpUhdDevice->startReceiving(_this->mStream);
        _this->mReceiving = true;
    }
}

void UHDSourceModule::stopHandler(void* ctx) {
    spdlog::debug("UHDSourceModule::stopHandler");
    UHDSourceModule* _this = (UHDSourceModule*)ctx;
    // TODO: stop receiving and cleanup resources
    if (_this->mpUhdDevice) {
        spdlog::debug("stop receiving data");
        _this->mpUhdDevice->stopReceiving();
    }
    _this->mReceiving = false;
}

void UHDSourceModule::tuneHandler(double freq, void* ctx) {
    spdlog::debug("UHDSourceModule::tuneHandler: freq = {0}", freq);
    UHDSourceModule* _this = (UHDSourceModule*)ctx;
    if (_this->mpUhdDevice) {
        _this->mpUhdDevice->setCenterFrequency(freq);
    }
}

void UHDSourceModule::openUHDDevice(void* ctx) {
    UHDSourceModule* _this = (UHDSourceModule*)ctx;
    if (_this->mDevices.empty() || (_this->mDeviceId < 1)) {
        spdlog::info("select a device");
        _this->mpUhdDevice = nullptr;
        return;
    }

    _this->mpUhdDevice = std::make_unique<UHDDevice>(_this->mDevices.at(_this->mDeviceId));
    _this->mpUhdDevice->open();
    if (!_this->mpUhdDevice->isOpen()) {
        spdlog::error("could not open device with serial {0}", _this->mDevices.at(_this->mDeviceId).serial());
        _this->mpUhdDevice = nullptr;
        return;
    }

    _this->mpUhdDevice->setChannelIndex(0);
    _this->mpUhdDevice->setRxGain(50);
    //_this->mpUhdDevice->set_rx_bandwidth();
    spdlog::debug("devie opened: index = {0}, serial = {1}", _this->mDeviceId, _this->mDevices.at(_this->mDeviceId).serial());
}

void UHDSourceModule::findUHDDevices() {
    spdlog::debug("UHDSourceModule::findUHDDevices");

    mDevices.clear();
    // TODO: use arguments to filter devices, filter in GUI only (may waste time finding all devices all the time)?
    const uhd::device_addr_t args("");
    uhd::device_addrs_t device_addrs = uhd::device::find(append_findall(args));
    if (device_addrs.empty()) {
        spdlog::info("No UHD Devices Found");
    }

    for (const auto& device_addr : device_addrs) {
        const int index = mDevices.add(Device{});
        Device& device = mDevices.at(index);
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

    if (!mDevices.empty()) {
        mDevices.sortBySerial();
        spdlog::info("found {0} device(s)", mDevices.size());
    }
}

std::string UHDSourceModule::getDeviceListString() const {
    std::stringstream deviceList;
    for (const auto& device : mDevices) {
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

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    g_config.setPath(options::opts.root + "/uhd_config.json");
    g_config.load(def);
    g_config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new UHDSourceModule(std::move(name));
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (UHDSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    g_config.disableAutoSave();
    g_config.save();
}