#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <librfnm/librfnm.h>
#include <core.h>
#include <utils/optionlist.h>
#include <atomic>

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
            return;
        }

        // Open the device
        librfnm* dev = new librfnm(librfnm_transport::LIBRFNM_TRANSPORT_USB, serial);

        // Define samplerates
        samplerates.clear();
        samplerates.define(61440000, "61.44 MHz", 2);
        samplerates.define(122880000, "122.88 MHz", 1);

        // Define daughterboards
        daughterboards.clear();
        for (int i = 0; i < 2; i++) {
            // If not present, skip
            if (!dev->s->hwinfo.daughterboard[i].board_id) { continue; }

            // Format the daughterboard name
            std::string name = (i ? "[SEC] " : "[PRI] ") + std::string(dev->s->hwinfo.daughterboard[i].user_readable_name);

            // Add the daughterboard to the list
            daughterboards.define(name, name, i);
        }

        // Load options (TODO)
        srId = samplerates.keyId(61440000);
        dgbId = 0;

        // Select the daughterboard
        selectDaughterboard(dev, 0);

        // Update samplerate
        sampleRate = samplerates.key(srId);

        // Close device
        delete dev;

        // Save serial number
        selectedSerial = serial;
    }

    struct PathConfig {
        rfnm_rf_path path;
        int chId;
        uint16_t appliesCh;

        bool operator==(const PathConfig& b) const {
            return b.path == path;
        }
    };
    
    void selectDaughterboard(librfnm* dev, int id) {
        // If no daugherboard is populated, give up
        if (!dev->s->hwinfo.daughterboard[0].board_id && !dev->s->hwinfo.daughterboard[1].board_id) {
            flog::error("The selected device has no daughterboards");
            return;
        }

        // If the ID is not populated, select the other one
        if (id >= 2 || !dev->s->hwinfo.daughterboard[id].board_id) {
            selectDaughterboard(dev, 1 - id);
        }

        // Compute the channel offset
        int offset = 0;
        for (int i = 0; i < id; i++) {
            offset += dev->s->hwinfo.daughterboard[i].rx_ch_cnt;
        }

        // Define antenna paths by going through all channels
        paths.clear();
        int count = dev->s->hwinfo.daughterboard[id].rx_ch_cnt;
        for (int i = 0; i < count; i++) {
            // Go through each possible path
            for (int j = 0; j < 10; j++) {
                // If it's the null path, stop searching
                rfnm_rf_path path = dev->s->rx.ch[offset + i].path_possible[j];
                if (path == RFNM_PATH_NULL) { continue; }

                // Get the path
                PathConfig pc = { path, offset + i, (uint16_t)(1 << (offset + i + 8))};
                
                // If it's not in the list, add it
                if (!paths.valueExists(pc)) {
                    std::string name = librfnm::rf_path_to_string(pc.path);
                    std::string capName = name;
                    if (std::islower(capName[0])) { capName[0] = std::toupper(capName[0]); }
                    paths.define(name, capName, pc);
                }
            }

            // Get the preferred path
            PathConfig preferred_pc = { dev->s->rx.ch[offset + i].path_preferred, 0, 0 };

            // Make sure the path is accessible or give up
            if (!paths.valueExists(preferred_pc)) { continue; }
            
            // Set this channel as the channel of its prefered path (cursed af but lazy)
            const PathConfig& pc = paths.value(paths.valueId(preferred_pc));
            ((PathConfig*)&pc)->chId = offset + i;
            ((PathConfig*)&pc)->appliesCh = (uint16_t)(1 << (offset + i + 8));
        }

        // Dump antenna paths
        for (int i = 0; i < paths.size(); i++) {
            flog::debug("PATH[{}]: Name={}, Ch={}, Path={}", i, paths.name(i), paths.value(i).chId, (int)paths.value(i).path);
        }

        // Load configuration (TODO)
        selectedPath = paths.key(0);

        // Select antenna path
        selectPath(dev, id, selectedPath);

        // Save selected daughterboard
        dgbId = id;
    }

    void selectPath(librfnm* dev, int dgbId, const std::string& path) {
        // If the path doesn't exist, select the first path
        if (!paths.keyExists(path)) {
            selectPath(dev, dgbId, paths.key(0));
        }

        // Save selected path
        selectedPath = path;
        pathId = paths.keyId(path);
        currentPath = paths.value(pathId);

        // Define bandwidths
        bandwidths.clear();
        bandwidths.define(-1, "Auto", -1);
        for (int i = 1; i <= 100; i++) {
            char buf[128];
            sprintf(buf, "%d MHz", i);
            bandwidths.define(i, buf, i);
        }

        // Get gain range
        gainMin = dev->s->rx.ch[currentPath.chId].gain_range.min;
        gainMax = dev->s->rx.ch[currentPath.chId].gain_range.max;
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

        // Flush buffers
        _this->openDev->rx_flush();

        // Configure the device
        _this->openDev->s->rx.ch[_this->currentPath.chId].enable = RFNM_CH_ON;
        _this->openDev->s->rx.ch[_this->currentPath.chId].samp_freq_div_n = _this->samplerates[_this->srId];
        _this->openDev->s->rx.ch[_this->currentPath.chId].freq = _this->freq;
        _this->openDev->s->rx.ch[_this->currentPath.chId].gain = _this->gain;
        _this->openDev->s->rx.ch[_this->currentPath.chId].rfic_lpf_bw = 100;
        _this->openDev->s->rx.ch[_this->currentPath.chId].fm_notch = _this->fmNotch ? rfnm_fm_notch::RFNM_FM_NOTCH_ON : rfnm_fm_notch::RFNM_FM_NOTCH_OFF;
        _this->openDev->s->rx.ch[_this->currentPath.chId].path = _this->currentPath.path;
        rfnm_api_failcode fail = _this->openDev->set(_this->currentPath.appliesCh);
        if (fail != rfnm_api_failcode::RFNM_API_OK) {
            flog::error("Failed to configure device: {}", (int)fail);
        }

        // Start worker
        _this->run = true;
        _this->workerThread = std::thread(&RFNMSourceModule::worker, _this);
        
        _this->running = true;
        flog::info("RFNMSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        RFNMSourceModule* _this = (RFNMSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        
        // Stop worker
        _this->run = false;
        _this->stream.stopWriter();
        if (_this->workerThread.joinable()) { _this->workerThread.join(); }
        _this->stream.clearWriteStop();

        // Stop the RX streaming
        _this->openDev->rx_stream_stop();

        // Disable channel
        _this->openDev->s->rx.ch[_this->currentPath.chId].enable = RFNM_CH_OFF;
        _this->openDev->set(_this->currentPath.appliesCh);

        // Flush buffers
        _this->openDev->rx_flush();

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
            _this->openDev->s->rx.ch[_this->currentPath.chId].freq = freq;
            rfnm_api_failcode fail = _this->openDev->set(_this->currentPath.appliesCh);
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
            _this->select(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->sampleRate);
            // TODO: Save
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

        if (_this->daughterboards.size() > 1) {
            SmGui::LeftLabel("Daughterboard");
            SmGui::FillWidth();
            if (SmGui::Combo(CONCAT("##_rfnm_dgb_sel_", _this->name), &_this->dgbId, _this->daughterboards.txt)) {
                // Open the device
                librfnm* dev = new librfnm(librfnm_transport::LIBRFNM_TRANSPORT_USB, _this->selectedSerial);

                // Select the daughterboard
                _this->selectDaughterboard(dev, _this->dgbId);

                // Close device
                delete dev;

                // TODO: Save
            }
        }
        
        if (_this->paths.size() > 1) {
            SmGui::LeftLabel("Antenna Path");
            SmGui::FillWidth();
            if (SmGui::Combo(CONCAT("##_rfnm_path_sel_", _this->name), &_this->pathId, _this->paths.txt)) {
                // Open the device
                librfnm* dev = new librfnm(librfnm_transport::LIBRFNM_TRANSPORT_USB, _this->selectedSerial);

                // Select the atennna path
                _this->selectPath(dev, _this->dgbId, _this->paths.key(_this->pathId));

                // Close device
                delete dev;

                // TODO: Save
            }
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_rfnm_bw_sel_", _this->name), &_this->bwId, _this->bandwidths.txt)) {
            if (_this->running) {
                // TODO: Set
            }
            // TODO: Save
        }

        SmGui::LeftLabel("Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_rfnm_gain_", _this->name), &_this->gain, _this->gainMin, _this->gainMax)) {
            if (_this->running) {
                _this->openDev->s->rx.ch[_this->currentPath.chId].gain = _this->gain;
                rfnm_api_failcode fail = _this->openDev->set(_this->currentPath.appliesCh);
            }
            // TODO: Save
        }

        if (SmGui::Checkbox(CONCAT("FM Notch##_rfnm_", _this->name), &_this->fmNotch)) {
            if (_this->running) {
                _this->openDev->s->rx.ch[_this->currentPath.chId].fm_notch = _this->fmNotch ? rfnm_fm_notch::RFNM_FM_NOTCH_ON : rfnm_fm_notch::RFNM_FM_NOTCH_OFF;
                rfnm_api_failcode fail = _this->openDev->set(_this->currentPath.appliesCh);
            }
            // TODO: Save
        }
    }

    void worker() {
        librfnm_rx_buf* lrxbuf;
        int sampCount = bufferSize/4;
        uint8_t ch = (1 << currentPath.chId);

        // Define number of buffers per swap to maintain 200 fps
        int maxBufCount = STREAM_BUFFER_SIZE / sampCount;
        int bufCount = (sampleRate / sampCount) / 200;
        if (bufCount <= 0) { bufCount = 1; }
        if (bufCount > maxBufCount) { bufCount = maxBufCount; }

        int count = 0;
        while (run) {
            // Receive a buffer
            auto fail = openDev->rx_dqbuf(&lrxbuf, ch, 1000);
            if (fail == rfnm_api_failcode::RFNM_API_DQBUF_NO_DATA) {
                flog::error("Dequeue buffer didn't have any data");
                continue;
            }
            else if (fail) { break; }

            // Convert buffer to CF32
            volk_16i_s32f_convert_32f((float*)&stream.writeBuf[(count++)*sampCount], (int16_t*)lrxbuf->buf, 32768.0f, sampCount * 2);

            // Reque buffer
            openDev->rx_qbuf(lrxbuf);

            // Swap data
            if (count >= bufCount) {
                if (!stream.swap(count*sampCount)) { break; }
                count = 0;
            }
            
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
    OptionList<std::string, int> daughterboards;
    OptionList<std::string, PathConfig> paths;
    OptionList<int, int> bandwidths;
    OptionList<int, int> samplerates;
    int gainMin = 0;
    int gainMax = 0;
    int devId = 0;
    int dgbId = 0;
    int pathId = 0;
    int srId = 0;
    int bwId = 0;
    int gain = 0;
    bool fmNotch = false;
    std::string selectedSerial;
    librfnm* openDev;
    int bufferSize = -1;
    std::string selectedPath;
    PathConfig currentPath;
    librfnm_rx_buf rxBuf[LIBRFNM_MIN_RX_BUFCNT];

    std::atomic<bool> run = false;
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