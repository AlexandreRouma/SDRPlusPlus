#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <utils/optionlist.h>
#include <htra_api.h>
#include <atomic>

SDRPP_MOD_INFO{
    /* Name:            */ "harogic_source",
    /* Description:     */ "RFNM Source Module",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

#define CONCAT(a, b) ((std::string(a) + b).c_str())

class HarogicSourceModule : public ModuleManager::Instance {
public:
    HarogicSourceModule(std::string name) {
        this->name = name;

        sampleRate = 61440000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        // Refresh devices
        refresh();

        // Select first (TODO: Select from config)
        select("");

        sigpath::sourceManager.registerSource("Harogic", &handler);
    }

    ~HarogicSourceModule() {
        
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

private:
    void refresh() {
        devices.clear();
        
        // Set up the device parameters
        BootProfile_TypeDef profile = {};
        profile.PhysicalInterface = PhysicalInterface_TypeDef::USB;
        profile.DevicePowerSupply = DevicePowerSupply_TypeDef::USBPortOnly;

        // Working variables
        void* dev;
        BootInfo_TypeDef binfo;
        
        for (int i = 0; i < 128; i++) {
            // Attempt to open the device with the given ID
            int ret = Device_Open(&dev, i, &profile, &binfo);
            if (ret < 0) {
                flog::debug("Done searching: {}", ret);
                break;
            }

            // Create serial string
            char serial[1024];
            sprintf(serial, "%016X", binfo.DeviceInfo.DeviceUID);
            
            // Add the device to the list
            devices.define(serial, serial, i);

            // Close the device
            Device_Close(&dev);
        }
    }

    void select(const std::string& serial) {
        // If there are no devices, give up
        if (devices.empty()) {
            selectedSerial.clear();
            return;
        }

        // If the serial was not found, select the first available serial
        if (!devices.keyExists(serial)) {
            select(devices.key(0));
            return;
        }

        // Get the menu ID
        devId = devices.keyId(serial);
        selectedDevIndex = devices.value(devId);

        // Set up the device parameters
        BootProfile_TypeDef bprofile = {};
        bprofile.PhysicalInterface = PhysicalInterface_TypeDef::USB;
        bprofile.DevicePowerSupply = DevicePowerSupply_TypeDef::USBPortOnly;

        // Working variables
        BootInfo_TypeDef binfo;

        // Attempt to open the device by ID
        void* dev;
        int ret = Device_Open(&dev, selectedDevIndex, &bprofile, &binfo);
        if (ret < 0) {
            flog::error("Could not open device: {}", ret);
            return;
        }

        // Get the default streaming parameters to query some info
        IQS_Profile_TypeDef profile;
        IQS_ProfileDeInit(&dev, &profile);
        
        // Compute all available samplerates
        samplerates.clear();
        for (int i = 0; i < 8; i++) {
            double sr = profile.NativeIQSampleRate_SPS / (double)(1 << i);
            char buf[128];
            sprintf(buf, "%.02fMHz", sr / 1e6);
            samplerates.define(1 << i, buf, sr);
        }

        // Close the device
        Device_Close(&dev);

        // TODO: Load configuration
        sampleRate = samplerates.value(0);

        // Update the samplerate
        core::setInputSampleRate(sampleRate);

        // Save serial number
        selectedSerial = serial;
    }

    static void menuSelected(void* ctx) {
        HarogicSourceModule* _this = (HarogicSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("HarogicSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        HarogicSourceModule* _this = (HarogicSourceModule*)ctx;
        flog::info("HarogicSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        HarogicSourceModule* _this = (HarogicSourceModule*)ctx;
        if (_this->running) { return; }

        // Set up the device parameters
        BootProfile_TypeDef bprofile = {};
        bprofile.PhysicalInterface = PhysicalInterface_TypeDef::USB;
        bprofile.DevicePowerSupply = DevicePowerSupply_TypeDef::USBPortAndPowerPort;

        // Working variables
        BootInfo_TypeDef binfo;

        // Attempt to open the device by ID
        int ret = Device_Open(&_this->openDev, _this->selectedDevIndex, &bprofile, &binfo);
        if (ret < 0) {
            flog::error("Could not open device: {}", ret);
            return;
        }

        // Get the decimation amount
        int dec = _this->samplerates.key(_this->samplerates.valueId(_this->sampleRate));
        flog::debug("Using decimation factor: {}", dec);

        // Decide to use either 8 or 16bit samples
        _this->sampsInt8 = (_this->sampleRate > 64e6);

        // Configure the IQ stream
        IQS_ProfileDeInit(&_this->openDev, &_this->profile);
        _this->profile.CenterFreq_Hz = _this->freq;
        _this->profile.RefLevel_dBm = -30;
        _this->profile.DecimateFactor = dec;
        _this->profile.RxPort = ExternalPort;
        _this->profile.BusTimeout_ms = 100;
        _this->profile.TriggerSource = Bus; 
        _this->profile.TriggerMode = Adaptive;
        _this->profile.DataFormat = _this->sampsInt8 ? Complex8bit : Complex16bit;

        // Apply the configuration
        IQS_StreamInfo_TypeDef info;
        ret = IQS_Configuration(&_this->openDev, &_this->profile, &_this->profile, &info);
        if (ret < 0) {
            flog::error("Could not configure device: {}", ret);
            Device_Close(&_this->openDev);
            return;
        }

        // Save the stream configuration
        _this->bufferSize = info.PacketSamples;
        flog::debug("Got buffer size: {}", _this->bufferSize);

        // Start the stream
        ret = IQS_BusTriggerStart(&_this->openDev);
        if (ret < 0) {
            flog::error("Could not start stream: {}", ret);
            Device_Close(&_this->openDev);
            return;
        }

        // Start worker
        _this->run = true;
        _this->workerThread = std::thread(&HarogicSourceModule::worker, _this);
        
        _this->running = true;
        flog::info("HarogicSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        HarogicSourceModule* _this = (HarogicSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        // Stop worker
        _this->run = false;
        _this->stream.stopWriter();
        if (_this->workerThread.joinable()) { _this->workerThread.join(); }
        _this->stream.clearWriteStop();

        // Stop the stream
        IQS_BusTriggerStop(&_this->openDev);

        // Close the device
        Device_Close(&_this->openDev);

        flog::info("HarogicSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        HarogicSourceModule* _this = (HarogicSourceModule*)ctx;
        if (_this->running) {
            // Acquire device
            std::lock_guard<std::mutex> lck(_this->devMtx);
            
            // Update the frequency in the configuration
            _this->profile.CenterFreq_Hz = freq;

            // Configure the device
            IQS_StreamInfo_TypeDef info;
            int ret = IQS_Configuration(&_this->openDev, &_this->profile, &_this->profile, &info);
            if (ret < 0) {
                flog::error("Failed to apply tuning config: {}", ret);
            }


            // Re-trigger the stream
            ret = IQS_BusTriggerStart(&_this->openDev);
            if (ret < 0) {
                flog::error("Could not start stream: {}", ret);
            }
        }
        _this->freq = freq;
        flog::info("HarogicSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        HarogicSourceModule* _this = (HarogicSourceModule*)ctx;
        
        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_rfnm_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->sampleRate);
            // TODO: Save
        }

        if (SmGui::Combo(CONCAT("##_rfnm_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates.value(_this->srId);
            core::setInputSampleRate(_this->sampleRate);
            // TODO: Save
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_rfnm_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }
    }

    void worker() {
        // Allocate sample buffer
        int realSamps = bufferSize*2;
        IQStream_TypeDef iqs;

        // Define number of buffers per swap to maintain 200 fps
        int maxBufCount = STREAM_BUFFER_SIZE / bufferSize;
        int bufCount = (sampleRate / bufferSize) / 200;
        if (bufCount <= 0) { bufCount = 1; }
        if (bufCount > maxBufCount) { bufCount = maxBufCount; }
        int count = 0;

        flog::debug("Swapping will be done {} buffers at a time", bufCount);

        // Worker loop
        while (run) {
            // Read samples
            devMtx.lock();
            int ret = IQS_GetIQStream_PM1(&openDev, &iqs);
            devMtx.unlock();
            if (ret < 0) {
                if (ret == APIRETVAL_WARNING_BusTimeOut) {
                    flog::warn("Stream timed out");
                    continue;
                }
                else if (ret <= APIRETVAL_WARNING_IFOverflow && ret >= APIRETVAL_WARNING_ADCConfigError) {
                    // Just warnings, do nothing
                }
                else {
                    flog::error("Streaming error: {}", ret);
                    break;
                }
            }

            // Convert them to floating point
            if (sampsInt8) {
                volk_8i_s32f_convert_32f((float*)&stream.writeBuf[(count++)*bufferSize], (int8_t*)iqs.AlternIQStream, 128.0f, realSamps);
            }
            else {
                volk_16i_s32f_convert_32f((float*)&stream.writeBuf[(count++)*bufferSize], (int16_t*)iqs.AlternIQStream, 32768.0f, realSamps);
            }

            // Send them off if we have enough
            if (count >= bufCount) {
                count = 0;
                if (!stream.swap(bufferSize*bufCount)) { break; }
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

    OptionList<std::string, int> devices;
    OptionList<int, double> samplerates;
    int devId = 0;
    int srId = 0;
    std::string selectedSerial;
    int selectedDevIndex;

    void* openDev;
    IQS_Profile_TypeDef profile;

    int bufferSize;
    std::thread workerThread;
    std::atomic<bool> run = false;
    std::mutex devMtx;
    bool sampsInt8;
};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new HarogicSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (HarogicSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing here
}