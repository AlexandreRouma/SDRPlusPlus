#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <utils/optionlist.h>
#include <fstream>
#include <dlcr.h>

#ifdef DRAGONLABS_SOURCE_ENABLE_DEBUG
#include <dlcr_internal.h>
#include <lmx2572.h>
#endif

SDRPP_MOD_INFO{
    /* Name:            */ "dragonlabs_source",
    /* Description:     */ "Dragon Labs Source Module",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

#define CONCAT(a, b) ((std::string(a) + b).c_str())

class DragonLabsSourceModule : public ModuleManager::Instance {
public:
    DragonLabsSourceModule(std::string name) {
        this->name = name;

        // Load the register debugging values
        strcpy(clkRegStr, "00");
        strcpy(clkValStr, "--");
        strcpy(synRegStr, "00");
        strcpy(synValStr, "--");
        strcpy(adcRegStr, "00");
        strcpy(adcValStr, "--");
        strcpy(tunRegStr, "00");
        strcpy(tunValStr, "--");

        // Define the clock sources
        clockSources.define("internal", "Internal", DLCR_CLOCK_INTERNAL);
        clockSources.define("external", "External", DLCR_CLOCK_EXTERNAL);

        // Define the channels
        channels.define("chan1", "Channel 1", 0);
        channels.define("chan2", "Channel 2", 1);
        channels.define("chan3", "Channel 3", 2);
        channels.define("chan4", "Channel 4", 3);
        channels.define("chan5", "Channel 5", 4);
        channels.define("chan6", "Channel 6", 5);
        channels.define("chan7", "Channel 7", 6);
        channels.define("chan8", "Channel 8", 7);

        // Hardcode the samplerate
        sampleRate = 12.5e6;

        // Fill out the source handler
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

        sigpath::sourceManager.registerSource("Dragon Labs", &handler);
    }

    ~DragonLabsSourceModule() {
        
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

        // Get the list of devices
        dlcr_info_t* list;
        int count = dlcr_list_devices(&list);
        if (count < 0) {
            flog::error("Failed to list devices");
            return;
        }

        // Generate the menu list
        for (int i = 0; i < count; i++) {
            // Format device name
            std::string devName = "CR8-1725 ";
            devName += " [";
            devName += list[i].serial;
            devName += ']';

            // Save device
            devices.define(list[i].serial, devName, list[i].serial);
        }

        // Free the device list
        dlcr_free_device_list(list);
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

        // Save serial number
        selectedSerial = serial;
    }

    static void menuSelected(void* ctx) {
        DragonLabsSourceModule* _this = (DragonLabsSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("DragonLabsSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        DragonLabsSourceModule* _this = (DragonLabsSourceModule*)ctx;
        flog::info("DragonLabsSourceModule '{0}': Menu Deselect!", _this->name);
    }

    std::chrono::high_resolution_clock::time_point last;

    static void start(void* ctx) {
        DragonLabsSourceModule* _this = (DragonLabsSourceModule*)ctx;
        if (_this->running) { return; }

        // Open the device
        int err = dlcr_open(&_this->openDev, _this->selectedSerial.c_str());
        if (err) {
            flog::error("Failed to open device '{}': {}", _this->selectedSerial, err);
            return;
        }
        flog::debug("Device open");

        // Configure the device
        dlcr_enable_channel(_this->openDev, DLCR_CHAN_ALL);
        dlcr_set_freq(_this->openDev, DLCR_CHAN_ALL, _this->freq, _this->docal);
        dlcr_set_lna_gain(_this->openDev, DLCR_CHAN_ALL, _this->lnaGain);
        dlcr_set_mixer_gain(_this->openDev, DLCR_CHAN_ALL, _this->mixerGain);
        dlcr_set_vga_gain(_this->openDev, DLCR_CHAN_ALL, _this->vgaGain);

        // Start device
        dlcr_start(_this->openDev, 0/*TODO*/, callback, _this);
                
        _this->running = true;
        flog::info("DragonLabsSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        DragonLabsSourceModule* _this = (DragonLabsSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        // Stop device
        dlcr_stop(_this->openDev);

        // Close device
        dlcr_close(_this->openDev);

        flog::info("DragonLabsSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        DragonLabsSourceModule* _this = (DragonLabsSourceModule*)ctx;
        if (_this->running) {
            dlcr_set_freq(_this->openDev, DLCR_CHAN_ALL, freq, _this->docal);
        }
        _this->calibrated = false;
        _this->freq = freq;
        flog::info("DragonLabsSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    double lmxFreq = 100e6;

    static void menuHandler(void* ctx) {
        DragonLabsSourceModule* _this = (DragonLabsSourceModule*)ctx;
        
        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_dlcr_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->sampleRate);
            // TODO: Save
        }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_dlcr_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Clock Source");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_dlcr_clock_", _this->name), &_this->clockSourceId, _this->clockSources.txt)) {
            if (_this->running) {
                dlcr_set_clock_source(_this->openDev, _this->clockSources[_this->clockSourceId]);
            }
            // TODO: Save
        }

        SmGui::LeftLabel("Channel");
        SmGui::FillWidth();
        SmGui::Combo(CONCAT("##_dlcr_chan_", _this->name), &_this->channelId, _this->channels.txt);

        SmGui::LeftLabel("LNA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_dlcr_lna_gain_", _this->name), &_this->lnaGain, 0, 14)) {
            if (_this->running) {
                dlcr_set_lna_gain(_this->openDev, DLCR_CHAN_ALL, _this->lnaGain);
            }
            // TODO: Save
        }

        SmGui::LeftLabel("Mixer Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_dlcr_mixer_gain_", _this->name), &_this->mixerGain, 0, 15)) {
            if (_this->running) {
                dlcr_set_mixer_gain(_this->openDev, DLCR_CHAN_ALL, _this->mixerGain);
            }
            // TODO: Save
        }

        SmGui::LeftLabel("VGA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_dlcr_vga_gain_", _this->name), &_this->vgaGain, 0, 15)) {
            if (_this->running) {
                dlcr_set_vga_gain(_this->openDev, DLCR_CHAN_ALL, _this->vgaGain);
            }
            // TODO: Save
        }

#ifdef DRAGONLABS_SOURCE_ENABLE_DEBUG
        SmGui::Checkbox(CONCAT("Debug##_dlcr_debug_", _this->name), &_this->debug);

        if (_this->debug) {
            ImGui::Separator();

            SmGui::LeftLabel("Clockgen Reg");
            SmGui::FillWidth();
            SmGui::InputText(CONCAT("##_dlcr_clk_reg_", _this->name), _this->clkRegStr, 256);
            SmGui::LeftLabel("Clockgen Value");
            SmGui::FillWidth();
            SmGui::InputText(CONCAT("##_dlcr_clk_val_", _this->name), _this->clkValStr, 256);
            SmGui::FillWidth();
            if (SmGui::Button(CONCAT("Read##_dlcr_clk_rd_", _this->name))) {
                if (_this->running) {
                    uint8_t val;
                    dlcr_si5351c_read_reg(_this->openDev, std::stoi(_this->clkRegStr, NULL, 16), &val);
                    sprintf(_this->clkValStr, "%02X", val);
                }
            }
            SmGui::FillWidth();
            if (SmGui::Button(CONCAT("Write##_dlcr_clk_wr_", _this->name))) {
                if (_this->running) {
                    dlcr_si5351c_write_reg(_this->openDev, std::stoi(_this->clkRegStr, NULL, 16), std::stoi(_this->clkValStr, NULL, 16));
                }
            }

            ImGui::Separator();

            SmGui::LeftLabel("Synth Reg");
            SmGui::FillWidth();
            SmGui::InputText(CONCAT("##_dlcr_syn_reg_", _this->name), _this->synRegStr, 256);
            SmGui::LeftLabel("Synth Value");
            SmGui::FillWidth();
            SmGui::InputText(CONCAT("##_dlcr_syn_val_", _this->name), _this->synValStr, 256);
            SmGui::FillWidth();
            if (SmGui::Button(CONCAT("Read##_dlcr_syn_rd_", _this->name))) {
                if (_this->running) {
                    uint16_t val;
                    dlcr_lmx2572_read_reg(_this->openDev, std::stoi(_this->synRegStr, NULL, 16), &val);
                    sprintf(_this->synValStr, "%02X", val);
                }
            }
            SmGui::FillWidth();
            if (SmGui::Button(CONCAT("Write##_dlcr_syn_wr_", _this->name))) {
                if (_this->running) {
                    dlcr_lmx2572_write_reg(_this->openDev, std::stoi(_this->synRegStr, NULL, 16), std::stoi(_this->synValStr, NULL, 16));
                }
            }

            ImGui::Separator();

            SmGui::LeftLabel("ADC Reg");
            SmGui::FillWidth();
            SmGui::InputText(CONCAT("##_dlcr_adc_reg_", _this->name), _this->adcRegStr, 256);
            SmGui::LeftLabel("ADC Value");
            SmGui::FillWidth();
            SmGui::InputText(CONCAT("##_dlcr_adc_val_", _this->name), _this->adcValStr, 256);
            SmGui::FillWidth();
            if (SmGui::Button(CONCAT("Read##_dlcr_adc_rd_", _this->name))) {
                if (_this->running) {
                    uint8_t val;
                    dlcr_mcp37211_read_reg(_this->openDev, std::stoi(_this->adcRegStr, NULL, 16), &val);
                    sprintf(_this->adcValStr, "%02X", val);
                }
            }
            SmGui::FillWidth();
            if (SmGui::Button(CONCAT("Write##_dlcr_adc_wr_", _this->name))) {
                if (_this->running) {
                    dlcr_mcp37211_write_reg(_this->openDev, std::stoi(_this->adcRegStr, NULL, 16), std::stoi(_this->adcValStr, NULL, 16));
                }
            }

            ImGui::Separator();

            SmGui::LeftLabel("Tuner Reg");
            SmGui::FillWidth();
            SmGui::InputText(CONCAT("##_dlcr_tun_reg_", _this->name), _this->tunRegStr, 256);
            SmGui::LeftLabel("Tuner Value");
            SmGui::FillWidth();
            SmGui::InputText(CONCAT("##_dlcr_tun_val_", _this->name), _this->tunValStr, 256);
            SmGui::FillWidth();
            if (SmGui::Button(CONCAT("Read##_dlcr_tun_rd_", _this->name))) {
                if (_this->running) {
                    uint8_t val[8];
                    dlcr_r860_read_reg(_this->openDev, DLCR_TUNER_ALL, std::stoi(_this->tunRegStr, NULL, 16), val);
                    sprintf(_this->tunValStr, "%02X", val[0]);
                    for (int i = 0; i < 8; i++) {
                        printf("TUNER[%d][0x%02X] = 0x%02X\n", i, std::stoi(_this->tunRegStr, NULL, 16), val[i]);
                    }
                }
            }
            SmGui::FillWidth();
            if (SmGui::Button(CONCAT("Write##_dlcr_tun_wr_", _this->name))) {
                if (_this->running) {
                    dlcr_r860_write_reg(_this->openDev, DLCR_TUNER_ALL, std::stoi(_this->tunRegStr, NULL, 16), std::stoi(_this->tunValStr, NULL, 16));
                }
            }

            ImGui::Separator();

            SmGui::LeftLabel("Synth Freq");
            SmGui::FillWidth();
            ImGui::InputDouble("##_dlcr_synth_freq", &_this->synthFreq);
            SmGui::FillWidth();
            if (SmGui::Button(CONCAT("Tune##_dlcr_synth_freq", _this->name))) {
                if (_this->running) {
                    lmx2572_tune(_this->openDev, _this->synthFreq);
                }
            }
            SmGui::FillWidth();
            if (SmGui::Button(CONCAT("Shutdown##_dlcr_synth_freq", _this->name))) {
                if (_this->running) {
                    lmx2572_shutdown(_this->openDev);
                }
            }

            ImGui::Separator();

            SmGui::Checkbox("Calibrate##_dlcr_cal", &_this->docal);

            ImGui::Separator();
        }
#endif
    }

    static void callback(dlcr_complex_t* samples[DLCR_CHANNEL_COUNT], size_t count, size_t drops, void* ctx) {
        DragonLabsSourceModule* _this = (DragonLabsSourceModule*)ctx;
        
        // Copy the data to the stream
        memcpy(_this->stream.writeBuf, samples[_this->channelId], count * sizeof(dsp::complex_t));

        // Send the samples
        _this->stream.swap(count);
    }

    std::string name;
    bool enabled = true;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq = 100e6;

    OptionList<std::string, std::string> devices;

    bool debug = false;
    char clkRegStr[256];
    char clkValStr[256];
    char synRegStr[256];
    char synValStr[256];
    char adcRegStr[256];
    char adcValStr[256];
    char tunRegStr[256];
    char tunValStr[256];
    double synthFreq = 100e6;
    bool docal = false;
    int devId = 0;
    int clockSourceId = 0;
    int channelId = 0;
    int lnaGain = 0;
    int mixerGain = 0;
    int vgaGain = 0;
    std::string selectedSerial;
    dlcr_t* openDev = NULL;

    bool calibrated = false;

    dsp::stream<dsp::complex_t> stream;
    OptionList<std::string, dlcr_clock_t> clockSources;
    OptionList<std::string, int> channels;
};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new DragonLabsSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (DragonLabsSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing here
}