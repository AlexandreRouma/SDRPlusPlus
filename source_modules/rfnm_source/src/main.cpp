#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <librfnm.h>
#include <core.h>
#include <utils/optionlist.h>

SDRPP_MOD_INFO{
    /* Name:            */ "rfnm_source",
    /* Description:     */ "RFNM Source Module",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

#define CONCAT(a, b) ((std::string(a) + b).c_str())

class RFNMSourceModule : public ModuleManager::Instance {
public:
    RFNMSourceModule(std::string name) {
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

        sigpath::sourceManager.registerSource("RFNM", &handler);
    }

    ~RFNMSourceModule() {
        
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
        auto list = librfnm::find(librfnm_transport::LIBRFNM_TRANSPORT_USB);
        for (const auto& info : list) {
            // Format device name
            std::string devName = "RFNM ";
            devName += info.motherboard.user_readable_name;
            devName += " [";
            devName += (char*)info.motherboard.serial_number;
            devName += ']';

            // Save device
            devices.define((char*)info.motherboard.serial_number, devName, (char*)info.motherboard.serial_number);
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
        }

        // // Open the device
        // librfnm* dev = new librfnm(librfnm_transport::LIBRFNM_TRANSPORT_USB, serial);

        // Define bandwidths
        bandwidths.clear();
        bandwidths.define(-1, "Auto", -1);
        for (int i = 1; i <= 100; i++) {
            char buf[128];
            sprintf(buf, "%d MHz", i);
            bandwidths.define(i, buf, i);
        }

        // Get gain range
        gainMin = -30;//dev->librfnm_s->rx.ch[0].gain_range.min;
        gainMax = 60;//dev->librfnm_s->rx.ch[0].gain_range.max;

        // // Close device
        // delete dev;

        // Define samplerates
        samplerates.clear();
        samplerates.define(61440000, "61.44 MHz", 2);
        samplerates.define(122880000, "122.88 MHz", 1);

        // TODO: Load options
        srId = samplerates.keyId(61440000);
        bwId = bandwidths.nameId("Auto");
        gain = 0;

        // Update samplerate
        sampleRate = samplerates.key(srId);

        // Save serial number
        selectedSerial = serial;
    }

    static void menuSelected(void* ctx) {
        RFNMSourceModule* _this = (RFNMSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("RFNMSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        RFNMSourceModule* _this = (RFNMSourceModule*)ctx;
        flog::info("RFNMSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        RFNMSourceModule* _this = (RFNMSourceModule*)ctx;
        if (_this->running) { return; }

        // Open the device
        _this->openDev = new librfnm(librfnm_transport::LIBRFNM_TRANSPORT_USB, _this->selectedSerial);

        // Configure the device
        _this->openDev->librfnm_s->rx.ch[0].enable = RFNM_CH_ON;
        _this->openDev->librfnm_s->rx.ch[0].samp_freq_div_n = _this->samplerates[_this->srId];
        _this->openDev->librfnm_s->rx.ch[0].freq = _this->freq;
        _this->openDev->librfnm_s->rx.ch[0].gain = _this->gain;
        _this->openDev->librfnm_s->rx.ch[0].rfic_lpf_bw = 100;
        _this->openDev->librfnm_s->rx.ch[0].fm_notch = _this->fmNotch ? rfnm_fm_notch::RFNM_FM_NOTCH_ON : rfnm_fm_notch::RFNM_FM_NOTCH_OFF;
        _this->openDev->librfnm_s->rx.ch[0].path = _this->openDev->librfnm_s->rx.ch[0].path_preferred;
        rfnm_api_failcode fail = _this->openDev->set(LIBRFNM_APPLY_CH0_RX);
        if (fail != rfnm_api_failcode::RFNM_API_OK) {
            flog::error("Failed to configure device: {}", (int)fail);
        }

        // Configure the stream
        _this->bufferSize = -1;
        _this->openDev->rx_stream(librfnm_stream_format::LIBRFNM_STREAM_FORMAT_CS16, &_this->bufferSize);
        if (_this->bufferSize <= 0) {
            flog::error("Failed to configure stream");
        }

        // Allocate and queue buffers
        flog::debug("BUFFER SIZE: {}", _this->bufferSize);
        for (int i = 0; i < LIBRFNM_MIN_RX_BUFCNT; i++) {
            _this->rxBuf[i].buf = dsp::buffer::alloc<uint8_t>(_this->bufferSize);
            _this->openDev->rx_qbuf(&_this->rxBuf[i]);
        }

        // Start worker
        _this->workerThread = std::thread(&RFNMSourceModule::worker, _this);
        
        _this->running = true;
        flog::info("RFNMSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        RFNMSourceModule* _this = (RFNMSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        // Stop worker
        _this->stream.stopWriter();
        if (_this->workerThread.joinable()) { _this->workerThread.join(); }
        _this->stream.clearWriteStop();

        // Disable channel
        _this->openDev->librfnm_s->rx.ch[0].enable = RFNM_CH_ON;
        _this->openDev->set(LIBRFNM_APPLY_CH0_RX);

        // Close device
        delete _this->openDev;

        // Free buffers
        for (int i = 0; i < LIBRFNM_MIN_RX_BUFCNT; i++) {
            dsp::buffer::free(_this->rxBuf[i].buf);
        }

        flog::info("RFNMSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        RFNMSourceModule* _this = (RFNMSourceModule*)ctx;
        if (_this->running) {
            _this->openDev->librfnm_s->rx.ch[0].freq = freq;
            rfnm_api_failcode fail = _this->openDev->set(LIBRFNM_APPLY_CH0_RX);
            if (fail != rfnm_api_failcode::RFNM_API_OK) {
                flog::error("Failed to tune: {}", (int)fail);
            }
        }
        _this->freq = freq;
        flog::info("RFNMSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        RFNMSourceModule* _this = (RFNMSourceModule*)ctx;
        
        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_rfnm_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            // TODO: Select
            // TODO: Save
            core::setInputSampleRate(_this->sampleRate);
        }

        if (SmGui::Combo(CONCAT("##_rfnm_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates.key(_this->srId);
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

        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_rfnm_bw_sel_", _this->name), &_this->bwId, _this->bandwidths.txt)) {
            // TODO: Save
        }

        SmGui::LeftLabel("Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_rfnm_gain_", _this->name), &_this->gain, _this->gainMin, _this->gainMax)) {
            if (_this->running) {
                _this->openDev->librfnm_s->rx.ch[0].gain = _this->gain;
                rfnm_api_failcode fail = _this->openDev->set(LIBRFNM_APPLY_CH0_RX);
            }
            // TODO: Save
        }

        if (SmGui::Checkbox(CONCAT("FM Notch##_rfnm_", _this->name), &_this->fmNotch)) {
            if (_this->running) {
                _this->openDev->librfnm_s->rx.ch[0].fm_notch = _this->fmNotch ? rfnm_fm_notch::RFNM_FM_NOTCH_ON : rfnm_fm_notch::RFNM_FM_NOTCH_OFF;
                rfnm_api_failcode fail = _this->openDev->set(LIBRFNM_APPLY_CH0_RX);
            }
            // TODO: Save
        }
    }

    void worker() {
        librfnm_rx_buf* lrxbuf;
        int sampCount = bufferSize/4;

        // TODO: Define number of buffers per swap to maintain 200 fps

        while (true) {
            // Receive a buffer
            auto fail = openDev->rx_dqbuf(&lrxbuf, LIBRFNM_CH0, 1000);
            if (fail == rfnm_api_failcode::RFNM_API_DQBUF_NO_DATA) {
                flog::error("Dequeue buffer didn't have any data");
                continue;
            }
            else if (fail) { break; }

            // Convert buffer to CF32
            volk_16i_s32f_convert_32f((float*)stream.writeBuf, (int16_t*)lrxbuf->buf, 32768.0f, sampCount * 2);

            // Reque buffer
            openDev->rx_qbuf(lrxbuf);

            // Swap data
            if (!stream.swap(sampCount)) { break; }
        }

        flog::debug("Worker exiting");
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;

    OptionList<std::string, std::string> devices;
    OptionList<int, int> bandwidths;
    OptionList<int, int> samplerates;
    int gainMin = 0;
    int gainMax = 0;

    int devId = 0;
    int srId = 0;
    int bwId = 0;
    int gain = 0;
    bool fmNotch = false;
    std::string selectedSerial;
    librfnm* openDev;
    int bufferSize = -1;
    librfnm_rx_buf rxBuf[LIBRFNM_MIN_RX_BUFCNT];

    std::thread workerThread;
};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RFNMSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (RFNMSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    // Nothing here
}