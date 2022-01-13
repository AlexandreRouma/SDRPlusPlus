#include "config.h"
#include "json.hpp"
#include "module.h"
#include "options.h"
#include "signal_path/source.h"
#include "signal_path/signal_path.h"

#include "uhd.h"
#include "uhd/device.hpp"

#include <map>
#include <set>
#include <string>

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
    uhd::device_addr_t append_findall(const uhd::device_addr_t& device_args)
    {
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
    typedef std::map<std::string, std::set<std::string>> device_multi_addrs_t;
    typedef std::map<std::string, device_multi_addrs_t> device_addrs_filtered_t;

    // SourceHandler
    static void menuHandler(void* ctx);
    static void selectHandler(void* ctx);
    static void deselectHandler(void* ctx);
    static void startHandler(void* ctx);
    static void stopHandler(void* ctx);
    static void tuneHandler(double freq, void* ctx);

    void findUSRPDevices();

    bool mEnabled;
    std::string mName;
    dsp::stream<dsp::complex_t> mStream;
    SourceManager::SourceHandler mHandler;
    device_addrs_filtered_t mAttachedDevices;
};

UHDSourceModule::UHDSourceModule(std::string name)
    : mEnabled(false)
    , mName(std::move(name)) {
    mHandler.ctx = this;
    mHandler.selectHandler = &UHDSourceModule::selectHandler;
    mHandler.deselectHandler = &UHDSourceModule::deselectHandler;
    mHandler.menuHandler = &UHDSourceModule::menuHandler;
    mHandler.startHandler = &UHDSourceModule::startHandler;
    mHandler.stopHandler = &UHDSourceModule::stopHandler;
    mHandler.tuneHandler = &UHDSourceModule::tuneHandler;
    mHandler.stream = &mStream;

    g_config.acquire();
    // access config
    g_config.release();

    findUSRPDevices();

    sigpath::sourceManager.registerSource("UHD", &mHandler);
}

UHDSourceModule::~UHDSourceModule() {
    //stop(this);
    // de-initialize UHD
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
    spdlog::debug("UHDSourceModule::menuHandler");
    UHDSourceModule* _this = (UHDSourceModule*)ctx;
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
}

void UHDSourceModule::stopHandler(void* ctx) {
    spdlog::debug("UHDSourceModule::stopHandler");
    UHDSourceModule* _this = (UHDSourceModule*)ctx;
}

void UHDSourceModule::tuneHandler(double freq, void* ctx) {
    spdlog::debug("UHDSourceModule::tuneHandler");
    UHDSourceModule* _this = (UHDSourceModule*)ctx;
}

void UHDSourceModule::findUSRPDevices() {
    // https://github.com/EttusResearch/uhd/blob/master/host/utils/uhd_find_devices.cpp
    spdlog::debug("UHDSourceModule::findUSRPDevices");
    // discover the usrps and print the results
    const uhd::device_addr_t args("");
    uhd::device_addrs_t device_addrs = uhd::device::find(append_findall(args));
    if (device_addrs.empty()) {
        spdlog::info("No UHD Devices Found");
        return;
    }

    for (auto it = device_addrs.begin(); it != device_addrs.end(); ++it) {
        std::string serial    = (*it)["serial"];
        mAttachedDevices[serial] = device_multi_addrs_t();
        for (const std::string& key : it->keys()) {
            if (key != "serial") {
                mAttachedDevices[serial][key].insert(it->get(key));
            }
        }
        for (auto sit = it + 1; sit != device_addrs.end();) {
            if ((*sit)["serial"] == serial) {
                for (const std::string& key : sit->keys()) {
                    if (key != "serial") {
                        mAttachedDevices[serial][key].insert(sit->get(key));
                    }
                }
                sit = device_addrs.erase(sit);
            } else {
                sit++;
            }
        }
    }

    int i = 0;
    for (auto & found_device : mAttachedDevices) {
        spdlog::info("--------------------------------------------------");
        spdlog::info("-- UHD Device {0}", i);
        spdlog::info("--------------------------------------------------");
        std::stringstream ss;
        ss << "Device Address:" << std::endl;
        ss << boost::format("    serial: %s") % found_device.first << std::endl;
        for (auto & mit : found_device.second) {
            for (auto vit = mit.second.begin(); vit != mit.second.end(); ++vit) {
                ss << boost::format("    %s: %s") % mit.first % *vit << std::endl;
            }
        }
        spdlog::info("{0}", ss.str());
        i++;
    }
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