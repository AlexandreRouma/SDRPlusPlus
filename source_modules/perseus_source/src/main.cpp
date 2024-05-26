#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <gui/widgets/stepped_slider.h>
#include <perseus-sdr.h>
#include <utils/optionlist.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "perseus_source",
    /* Description:     */ "Perseus SDR source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

#define MAX_SAMPLERATE_COUNT    128

ConfigManager config;

class PerseusSourceModule : public ModuleManager::Instance {
public:
    PerseusSourceModule(std::string name) {
        this->name = name;

        sampleRate = 768000;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        perseus_set_debug(9);

        refresh();

        config.acquire();
        std::string serial = config.conf["device"];
        config.release();
        select(serial);

        sigpath::sourceManager.registerSource("Perseus", &handler);
    }

    ~PerseusSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("Perseus");
        if (libInit) { perseus_exit(); }
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

    void refresh() {
        // Re-initialize driver
        if (libInit) { perseus_exit(); }
        int devCount = perseus_init();
        if (devCount < 0) {
            libInit = false;
            flog::error("Could not initialize libperseus: {}", perseus_errorstr());
            return;
        }
        libInit = true;

        // Open each device to get the serial number
        for (int i = 0; i < devCount; i++) {
            // Open device
            perseus_descr* dev = perseus_open(i);
            if (!dev) {
                flog::error("Failed to open Perseus device with ID {}: {}", i, perseus_errorstr());
                continue;
            }

            // Load firmware
            int err = perseus_firmware_download(dev, NULL);
            if (err) {
                flog::error("Could not upload firmware to device {}: {}", i, perseus_errorstr());
                perseus_close(dev);
                continue;
            }

            // Get info
            eeprom_prodid prodId;
            err = perseus_get_product_id(dev, &prodId);
            if (err) {
                flog::error("Could not getproduct info from device {}: {}", i, perseus_errorstr());
                perseus_close(dev);
                continue;
            }

            // Create entry
            char serial[128];
            char buf[128];
            sprintf(serial, "%05d", (int)prodId.sn);
            sprintf(buf, "Perseus %d.%d [%s]", (int)prodId.hwver, (int)prodId.hwrel, serial);
            devList.define(serial, buf, i);

            // Close device
            perseus_close(dev);
        }
    }

    void select(const std::string& serial) {
        // If there are no devices, give up
        if (devList.empty()) { 
            selectedSerial.clear();
            return;
        }

        // If the serial number is not available, select first instead
        if (!devList.keyExists(serial)) {
            select(devList.key(0));
            return;
        }

        // Open device
        selectedSerial = serial;
        selectedPerseusId = devList.value(devList.keyId(serial));
        perseus_descr* dev = perseus_open(selectedPerseusId);
        if (!dev) {
            flog::error("Failed to open device {}: {}", selectedPerseusId, perseus_errorstr());
            selectedSerial.clear();
            return;
        }

        // Load firmware
        int err = perseus_firmware_download(dev, NULL);
        if (err) {
            flog::error("Could not upload firmware to device: {}", perseus_errorstr());
            perseus_close(dev);
            selectedSerial.clear();
            return;
        }

        // Get info
        eeprom_prodid prodId;
        err = perseus_get_product_id(dev, &prodId);
        if (err) {
            flog::error("Could not getproduct info from device: {}", perseus_errorstr());
            perseus_close(dev);
            selectedSerial.clear();
            return;
        }

        // List samplerates
        srList.clear();
        int samplerates[MAX_SAMPLERATE_COUNT];
        memset(samplerates, 0, sizeof(int)*MAX_SAMPLERATE_COUNT);
        err = perseus_get_sampling_rates(dev, samplerates, MAX_SAMPLERATE_COUNT);
        if (err) {
            flog::error("Could not get samplerate list: {}", perseus_errorstr());
            perseus_close(dev);
            selectedSerial.clear();
            return;
        }
        for (int i = 0; i < MAX_SAMPLERATE_COUNT; i++) {
            if (!samplerates[i]) { break; }
            srList.define(samplerates[i], getBandwdithScaled(samplerates[i]), samplerates[i]);
        }

        // TODO: List attenuator values

        // Load options
        srId = 0;
        dithering = false;
        preamp = false;
        preselector = true;
        atten = 0;
        config.acquire();
        if (config.conf["devices"][selectedSerial].contains("samplerate")) {
            int sr = config.conf["devices"][selectedSerial]["samplerate"];
            if (srList.keyExists(sr)) {
                srId = srList.keyId(sr);
            }
        }
        if (config.conf["devices"][selectedSerial].contains("dithering")) {
            dithering = config.conf["devices"][selectedSerial]["dithering"];
        }
        if (config.conf["devices"][selectedSerial].contains("preamp")) {
            preamp = config.conf["devices"][selectedSerial]["preamp"];
        }
        if (config.conf["devices"][selectedSerial].contains("preselector")) {
            preselector = config.conf["devices"][selectedSerial]["preselector"];
        }
        if (config.conf["devices"][selectedSerial].contains("attenuation")) {
            atten = config.conf["devices"][selectedSerial]["attenuation"];
        }
        config.release();

        // Update samplerate
        sampleRate = srList[srId];

        // Close device
        perseus_close(dev);
    }

private:
    std::string getBandwdithScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            sprintf(buf, "%.1lfMHz", bw / 1000000.0);
        }
        else if (bw >= 1000.0) {
            sprintf(buf, "%.1lfKHz", bw / 1000.0);
        }
        else {
            sprintf(buf, "%.1lfHz", bw);
        }
        return std::string(buf);
    }

    static void menuSelected(void* ctx) {
        PerseusSourceModule* _this = (PerseusSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("PerseusSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        PerseusSourceModule* _this = (PerseusSourceModule*)ctx;
        flog::info("PerseusSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        PerseusSourceModule* _this = (PerseusSourceModule*)ctx;
        if (_this->running) { return; }
        if (_this->selectedSerial.empty()) {
            flog::error("No device is selected");
            return;
        }
        
        // Open device
        _this->openDev = perseus_open(_this->selectedPerseusId);
        if (!_this->openDev) {
            flog::error("Failed to open device {}: {}", _this->selectedPerseusId, perseus_errorstr());
            return;
        }

        // Load firmware
        int err = perseus_firmware_download(_this->openDev, NULL);
        if (err) {
            flog::error("Could not upload firmware to device: {}", perseus_errorstr());
            perseus_close(_this->openDev);
            return;
        }

        // Set samplerate
        err = perseus_set_sampling_rate(_this->openDev, _this->sampleRate);
        if (err) {
            flog::error("Could not set samplerate: {}", perseus_errorstr());
            perseus_close(_this->openDev);
            return;
        }

        // Set options
        perseus_set_adc(_this->openDev, _this->dithering, _this->preamp);
        perseus_set_attenuator_in_db(_this->openDev, _this->atten);
        perseus_set_ddc_center_freq(_this->openDev, _this->freq, _this->preselector);

        // Start stream
        int idealBufferSize = _this->sampleRate / 200;
        int multipleOf1024 = std::clamp<int>(idealBufferSize / 1024, 1, 2);
        int bufferSize = multipleOf1024 * 1024;
        int bufferBytes = bufferSize*6;
        err = perseus_start_async_input(_this->openDev, bufferBytes, callback, _this);
        if (err) {
            flog::error("Could not start stream: {}", perseus_errorstr());
            perseus_close(_this->openDev);
            return;
        }

        _this->running = true;
        flog::info("PerseusSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        PerseusSourceModule* _this = (PerseusSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;

        // Stop stream
        _this->stream.stopWriter();
        perseus_stop_async_input(_this->openDev);
        _this->stream.clearWriteStop();

        // Close device
        perseus_close(_this->openDev);

        flog::info("PerseusSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        PerseusSourceModule* _this = (PerseusSourceModule*)ctx;
        if (_this->running) {
            perseus_set_ddc_center_freq(_this->openDev, freq, _this->preselector);
        }
        _this->freq = freq;
        flog::info("PerseusSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        PerseusSourceModule* _this = (PerseusSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_airspyhf_dev_sel_", _this->name), &_this->devId, _this->devList.txt)) {
            std::string serial = _this->devList.key(_this->devId);
            _this->select(serial);
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["device"] = serial;
            config.release(true);
        }

        if (SmGui::Combo(CONCAT("##_airspyhf_sr_sel_", _this->name), &_this->srId, _this->srList.txt)) {
            _this->sampleRate = _this->srList[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["samplerate"] = _this->sampleRate;
                config.release(true);
            }
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_airspyhf_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Attenuation");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_airspyhf_atten_", _this->name), &_this->atten, 0, 30, 10, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->running) {
                perseus_set_attenuator_in_db(_this->openDev, _this->atten);
            }
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["attenuation"] = _this->atten;
                config.release(true);
            }
        }

        if (SmGui::Checkbox(CONCAT("Preamp##_airspyhf_preamp_", _this->name), &_this->preamp)) {
            if (_this->running) {
                perseus_set_adc(_this->openDev, _this->dithering, _this->preamp);
            }
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["preamp"] = _this->preamp;
                config.release(true);
            }
        }

        if (SmGui::Checkbox(CONCAT("Dithering##_airspyhf_dither_", _this->name), &_this->dithering)) {
            if (_this->running) {
                perseus_set_adc(_this->openDev, _this->dithering, _this->preamp);
            }
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["dithering"] = _this->dithering;
                config.release(true);
            }
        }

        if (SmGui::Checkbox(CONCAT("Preselector##_airspyhf_presel_", _this->name), &_this->preselector)) {
            if (_this->running) {
                perseus_set_ddc_center_freq(_this->openDev, _this->freq, _this->preselector);
            }
            if (!_this->selectedSerial.empty()) {
                config.acquire();
                config.conf["devices"][_this->selectedSerial]["preselector"] = _this->preselector;
                config.release(true);
            }
        }
    }

    static int callback(void* buf, int bufferSize, void* ctx) {
        PerseusSourceModule* _this = (PerseusSourceModule*)ctx;
        uint8_t* samples = (uint8_t*)buf;
        int sampleCount = bufferSize / 6;
        for (int i = 0; i < sampleCount; i++) {
            int32_t re, im;
            re = *(samples++);
            re |= *(samples++) << 8;
            re |= *(samples++) << 16;
            re |= (re >> 23) * (0xFF << 24); // Sign extend
            im = *(samples++);
            im |= *(samples++) << 8;
            im |= *(samples++) << 16;
            im |= (im >> 23) * (0xFF << 24); // Sign extend
            _this->stream.writeBuf[i].re = ((float)re / (float)0x7FFFFF);
            _this->stream.writeBuf[i].im = ((float)im / (float)0x7FFFFF);
        }
        _this->stream.swap(sampleCount);
        return 0;
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    int sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    int devId = 0;
    int srId = 0;
    bool libInit = false;
    perseus_descr* openDev;
    std::string selectedSerial = "";
    int selectedPerseusId;
    float atten = 0;
    bool preamp = false;
    bool dithering = false;
    bool preselector = true;

    OptionList<std::string, int> devList;
    OptionList<int, int> srList;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/perseus_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new PerseusSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (PerseusSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}