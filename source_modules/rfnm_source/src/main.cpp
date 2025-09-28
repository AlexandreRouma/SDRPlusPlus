// SPDX-License-Identifier: GPL-3.0-or-later
//
// RFNM Source module for SDR++
// Ported to the current public API of librfnm:
//  - Uses rfnm::device and rfnm::rx_stream (no more direct dev->s access)
//  - Discovery via rfnm::device::find(transport)
//  - Capabilities via get_hwinfo() / get_rx_channel(...)
//  - Configuration via set_rx_channel_* (..., apply=true)
//  - Streaming via rx_stream::start/stop/read(...)
// Keep the original SDR++ glue (GUI, OptionList, streams) intact.

#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/smgui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <utils/optionlist.h>

#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <cctype>
#include <cstdint>
#include <cstdio>

#include <volk/volk.h>

// New librfnm public headers (replace the old <librfnm/librfnm.h>)
#include <librfnm/device.h>
#include <librfnm/rx_stream.h>
// enums, structs live across librfnm_api.h / rfnm_fw_api.h and are included by device.h

// Helper: translate rfnm_api_failcode into human-readable string
static const char* failcode_to_str(rfnm_api_failcode code) {
    switch (code) {
        case RFNM_API_OK: return "OK";
        case RFNM_API_PROBE_FAIL: return "Probe failed";
        case RFNM_API_TUNE_FAIL: return "Tune failed";
        case RFNM_API_GAIN_FAIL: return "Gain setting failed";
        case RFNM_API_TIMEOUT: return "Timeout";
        case RFNM_API_USB_FAIL: return "USB communication failed";
        case RFNM_API_DQBUF_OVERFLOW: return "DQ buffer overflow";
        case RFNM_API_NOT_SUPPORTED: return "Operation not supported";
        case RFNM_API_SW_UPGRADE_REQUIRED: return "Firmware upgrade required";
        case RFNM_API_DQBUF_NO_DATA: return "No data in DQ buffer";
        case RFNM_API_MIN_QBUF_CNT_NOT_SATIFIED: return "Minimum QBUF count not satisfied";
        case RFNM_API_MIN_QBUF_QUEUE_FULL: return "QBUF queue full";
        default: return "Unknown error";
    }
}

SDRPP_MOD_INFO{
    "rfnm_source",
    "RFNM Source Module",
    "Ryzerth",
    0, 1, 0
};


#define CONCAT(a, b) ((std::string(a) + b).c_str())

class RFNMSourceModule : public ModuleManager::Instance {
public:
    RFNMSourceModule(std::string name) {
        this->name = name;

        // Keep the same defaults as the original file
        sampleRate = 61440000.0;
        freq = 100e6;
        gain = 0;
        fmNotch = false;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        // Refresh USB list and pick the first device
        refresh();
        select("");

        sigpath::sourceManager.registerSource("RFNM", &handler);
    }

    ~RFNMSourceModule() {}

    void postInit() {}

    void enable()  { enabled = true;  }
    void disable() { enabled = false; }
    bool isEnabled(){ return enabled;  }

private:
    // Path selection structure (UI-facing)
    struct PathConfig {
        enum rfnm_rf_path path;  // exact enum from librfnm headers
        int chId;                // RX channel index
        uint16_t appliesCh;      // commit/apply mask (we keep 1<<ch for clarity)

        bool operator==(const PathConfig& b) const {
            return b.path == path;
        }
    };

    // Convert enum path to a human label (use librfnm utility if available)
    static std::string pathToLabel(enum rfnm_rf_path p) {
        // Prefer library's stringify (present in your headers)
        return rfnm::device::rf_path_to_string(p);
    }

    // =======================
    // Device enumeration
    // =======================
    void refresh() {
        devices.clear();

        // Your headers expose:
        //   static std::vector<struct rfnm_dev_hwinfo> find(enum transport transport, std::string address="", int bind=0);
        // Use USB transport to discover connected devices
        auto list = rfnm::device::find(rfnm::transport::TRANSPORT_USB);

        for (const auto& hw : list) {
            // Build a human-friendly name: "RFNM <board name> [<serial>]"
            std::string serial = (const char*)hw.motherboard.serial_number;
            std::string devName = "RFNM ";
            devName += hw.motherboard.user_readable_name;
            devName += " [";
            devName += serial;
            devName += ']';

            devices.define(serial, devName, serial);
        }
    }

    // =======================
    // Device selection
    // =======================
    void select(const std::string& serial) {
        if (devices.empty()) {
            selectedSerial.clear();
            return;
        }

        if (!devices.keyExists(serial)) {
            select(devices.key(0));
            return;
        }

        // Open device just to populate UI options (samplerates/DB/paths/gain range)
        rfnm::device dev(rfnm::transport::TRANSPORT_USB, serial);

        // Sample rates: keep the same options as the original
        samplerates.clear();
        samplerates.define(61440000, "61.44 MHz", 2);
        samplerates.define(122880000, "122.88 MHz", 1);
        srId = samplerates.keyId(61440000);

        // Daughterboards list: via public hwinfo
        const struct rfnm_dev_hwinfo* hw = dev.get_hwinfo();
        daughterboards.clear();
        for (int i = 0; i < 2; i++) {
            if (!hw->daughterboard[i].board_id) continue;
            std::string name = (i ? "[SEC] " : "[PRI] ") + std::string(hw->daughterboard[i].user_readable_name);
            daughterboards.define(name, name, i);
        }
        dgbId = 0; // default to primary

        // Build the antenna paths/options for the chosen daughterboard
        selectDaughterboard(&dev, dgbId);

        // Set current sample rate value
        sampleRate = samplerates.key(srId);

        // Save chosen serial
        selectedSerial = serial;
    }

    // Build path list and gain range using public per-channel info
    void selectDaughterboard(rfnm::device* dev, int id) {
        const struct rfnm_dev_hwinfo* hw = dev->get_hwinfo();

        if (!hw->daughterboard[0].board_id && !hw->daughterboard[1].board_id) {
            flog::error("The selected device has no daughterboards");
            return;
        }
        if (id >= 2 || !hw->daughterboard[id].board_id) {
            selectDaughterboard(dev, 1 - id);
            return;
        }

        // Compute channel offset like original (sum of previous DB channel counts)
        int offset = 0;
        for (int i = 0; i < id; i++) offset += hw->daughterboard[i].rx_ch_cnt;

        // Enumerate antenna paths per channel using public channel info
        paths.clear();
        int count = hw->daughterboard[id].rx_ch_cnt;

        for (int i = 0; i < count; i++) {
            int ch = offset + i;
            const struct rfnm_api_rx_ch* chInfo = dev->get_rx_channel((uint32_t)ch);

            // Register all available paths on that channel
            for (int j = 0; j < 10; j++) {
                enum rfnm_rf_path p = chInfo->path_possible[j];
                if (p == RFNM_PATH_NULL) continue; // null/invalid marker in firmware API

                PathConfig pc{ p, ch, static_cast<uint16_t>(1u << ch) }; // commit mask: 1<<channel

                if (!paths.valueExists(pc)) {
                    std::string name = pathToLabel(pc.path);
                    std::string capName = name;
                    if (!capName.empty() && std::islower((unsigned char)capName[0])) capName[0] = std::toupper((unsigned char)capName[0]);
                    paths.define(name, capName, pc);
                }
            }

            // Try to keep the preferred path for this channel
            if (chInfo->path_preferred != RFNM_PATH_NULL) {
                PathConfig preferred_pc{ chInfo->path_preferred, ch, static_cast<uint16_t>(1u << ch) };
                if (paths.valueExists(preferred_pc)) {
                    const PathConfig& pc = paths.value(paths.valueId(preferred_pc));
                    // Ensure chId/appliesCh reflect this channel
                    ((PathConfig*)&pc)->chId = ch;
                    ((PathConfig*)&pc)->appliesCh = static_cast<uint16_t>(1u << ch);
                }
            }
        }

        // Log result (unchanged)
        for (int i = 0; i < paths.size(); i++) {
            flog::debug("PATH[{}]: Name={}, Ch={}, Path={}", i, paths.name(i), paths.value(i).chId, (int)paths.value(i).path);
        }

        // Default selection: first path
        selectedPath = paths.key(0);
        selectPath(dev, id, selectedPath);

        dgbId = id;
    }

    void selectPath(rfnm::device* dev, int dgbId, const std::string& path) {
        (void)dgbId; // mapping DB->channel offset is already applied above

        if (!paths.keyExists(path)) {
            selectPath(dev, dgbId, paths.key(0));
            return;
        }

        selectedPath = path;
        pathId = paths.keyId(path);
        currentPath = paths.value(pathId);

        // Bandwidth options (same placeholder list as original)
        bandwidths.clear();
        bandwidths.define(-1, "Auto", -1);
        for (int i = 1; i <= 100; i++) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%d MHz", i);
            bandwidths.define(i, buf, i);
        }

        // Gain range from public channel info
        const struct rfnm_api_rx_ch* chInfo = dev->get_rx_channel((uint32_t)currentPath.chId);
        gainMin = chInfo->gain_range.min;
        gainMax = chInfo->gain_range.max;
    }

    // =======================
    // Menu & lifecycle hooks
    // =======================
    static void menuSelected(void* ctx) {
        auto* _this = (RFNMSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("RFNMSourceModule '{}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        auto* _this = (RFNMSourceModule*)ctx;
        flog::info("RFNMSourceModule '{}': Menu Deselect!", _this->name);
    }

    // Map GUI sample rate choice to (m,n) divider for set_rx_channel_samp_freq_div().
    // Assuming base clock is 122.88 MHz: 122.88 -> (1,1), 61.44 -> (1,2).
    static void srToDividers(int sr, int16_t& m, int16_t& n) {
        switch (sr) {
            case 122880000: m = 1; n = 1; break;
            case 61440000:  m = 1; n = 2; break;
            default:        m = 1; n = 2; break; // safe default
        }
    }

    static void start(void* ctx) {
        auto* _this = (RFNMSourceModule*)ctx;
        if (_this->running) return;

        // Open the device
        _this->dev = std::make_unique<rfnm::device>(rfnm::transport::TRANSPORT_USB, _this->selectedSerial);

        // Apply initial configuration using public setters (apply=true does an immediate commit)
        const int ch = _this->currentPath.chId;

        // Sample-rate: translate to (m,n) (PATCH CORRECTION)
        int16_t m = 1, n = 2;
        srToDividers(_this->samplerates.key(_this->srId), m, n);
        _this->dev->set_rx_channel_samp_freq_div((uint32_t)ch, m, n, true);

        // Frequency / gain / filters / path
        _this->dev->set_rx_channel_freq((uint32_t)ch, (int64_t)_this->freq, true);
        _this->dev->set_rx_channel_gain((uint32_t)ch, (int8_t)_this->gain, true);
        _this->dev->set_rx_channel_rfic_lpf_bw((uint32_t)ch, 100, true);
        _this->dev->set_rx_channel_fm_notch((uint32_t)ch, _this->fmNotch ? RFNM_FM_NOTCH_ON : RFNM_FM_NOTCH_OFF, true);
        _this->dev->set_rx_channel_path((uint32_t)ch, _this->currentPath.path, true);

        // Create an RX stream on the selected channel (bitmask uses bit-per-channel)
        uint8_t ch_mask = (uint8_t)(1u << ch);
        _this->rx.reset(new rfnm::rx_stream(*_this->dev, ch_mask));

        // Start streaming
        auto rc = _this->rx->start();
        if (rc != RFNM_API_OK) {
            flog::error("RFNM: rx_stream start failed: {} ({})", (int)rc, failcode_to_str(rc));
        }

        // Prepare local buffer for int16 I/Q (CS16). SDR++ expects CF32; we convert on the fly.
        // We will request chunks sized to maintain ~200 FPS like the original logic.
        _this->prepareWorkerBuffers();

        // Start worker
        _this->run = true;
        _this->workerThread = std::thread(&RFNMSourceModule::worker, _this);

        _this->running = true;
        flog::info("RFNMSourceModule '{}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        auto* _this = (RFNMSourceModule*)ctx;
        if (!_this->running) return;
        _this->running = false;

        // Stop worker
        _this->run = false;
        _this->stream.stopWriter();
        if (_this->workerThread.joinable()) _this->workerThread.join();
        _this->stream.clearWriteStop();

        // Stop streaming and disable channel
        if (_this->rx) {
            _this->rx->stop();
            _this->rx.reset();
        }
        if (_this->dev) {
            // PATCH CORRECTION: rimossa la chiamata set_rx_channel_active qui
            _this->dev.reset();
        }

        flog::info("RFNMSourceModule '{}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        auto* _this = (RFNMSourceModule*)ctx;
        _this->freq = freq;
        if (_this->running && _this->dev) {
            const int ch = _this->currentPath.chId;
            _this->dev->set_rx_channel_freq((uint32_t)ch, (int64_t)freq, true);
        }
        flog::info("RFNMSourceModule '{}': Tune: {}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        auto* _this = (RFNMSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        // Device selector
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_rfnm_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->sampleRate);
        }

        // Samplerate selector
        if (SmGui::Combo(CONCAT("##_rfnm_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates.key(_this->srId);
            core::setInputSampleRate(_this->sampleRate);
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_rfnm_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        // Daughterboard selector (if multiple)
        if (_this->daughterboards.size() > 1) {
            SmGui::LeftLabel("Daughterboard");
            SmGui::FillWidth();
            if (SmGui::Combo(CONCAT("##_rfnm_dgb_sel_", _this->name), &_this->dgbId, _this->daughterboards.txt)) {
                rfnm::device dev(rfnm::transport::TRANSPORT_USB, _this->selectedSerial);
                _this->selectDaughterboard(&dev, _this->dgbId);
            }
        }

        // Antenna path selector (if multiple)
        if (_this->paths.size() > 1) {
            SmGui::LeftLabel("Antenna Path");
            SmGui::FillWidth();
            if (SmGui::Combo(CONCAT("##_rfnm_path_sel_", _this->name), &_this->pathId, _this->paths.txt)) {
                rfnm::device dev(rfnm::transport::TRANSPORT_USB, _this->selectedSerial);
                _this->selectPath(&dev, _this->dgbId, _this->paths.key(_this->pathId));
            }
        }

        if (_this->running) { SmGui::EndDisabled(); }

        // Bandwidth (PATCH CORRECTION: implementazione reale invece del TODO)
        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_rfnm_bw_sel_", _this->name), &_this->bwId, _this->bandwidths.txt)) {
            if (_this->running && _this->dev) {
                _this->dev->set_rx_channel_rfic_lpf_bw(_this->currentPath.chId, _this->bandwidths.key(_this->bwId), true);
            }
        }

        // Gain slider -> set immediately if running
        SmGui::LeftLabel("Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_rfnm_gain_", _this->name), &_this->gain, _this->gainMin, _this->gainMax)) {
            if (_this->running && _this->dev) {
                const int ch = _this->currentPath.chId;
                _this->dev->set_rx_channel_gain((uint32_t)ch, (int8_t)_this->gain, true);
            }
        }

        // FM notch checkbox -> set immediately if running
        if (SmGui::Checkbox(CONCAT("FM Notch##_rfnm_", _this->name), &_this->fmNotch)) {
            if (_this->running && _this->dev) {
                const int ch = _this->currentPath.chId;
                _this->dev->set_rx_channel_fm_notch((uint32_t)ch,
                    _this->fmNotch ? RFNM_FM_NOTCH_ON : RFNM_FM_NOTCH_OFF, true);
            }
        }
    }

    // =======================
    // Worker thread
    // =======================
    void prepareWorkerBuffers() {
        // Keep the original behavior: aim ~200 FPS
        // We don't know the exact device chunk, so we compute a sensible read size.
        // We'll read 'elems_to_read' complex samples per call for our single channel.
        // stream.writeBuf expects CF32; we convert from CS16 after each read.
        const int bytes_per_cs16_sample = 4; // 2 bytes I + 2 bytes Q
        int sampCount = 1 << 15;             // 32768 complex samples per read (tunable)
        // Make sure this maps well to SDR++ STREAM_BUFFER_SIZE policy
        (void)bytes_per_cs16_sample;
        elemsPerRead = sampCount;
    }

    void worker() {
        // Single-channel streaming. rx_stream::read wants an array of output buffers
        // (multi-channel), but here we map only our selected channel to one buffer.
        const int ch = currentPath.chId;

        // Temporary raw buffer: CS16 interleaved
        std::vector<int16_t> tmpIQ(elemsPerRead * 2); // I & Q per sample

        // PATCH CORRECTION: semplificazione del buffer array
        std::vector<void*> buffs = {tmpIQ.data()};

        while (run) {
            size_t elems_read = 0;
            uint64_t ts_ns = 0;
            auto rc = rx->read((void* const*)buffs.data(), (size_t)elemsPerRead, elems_read, ts_ns, 20000);
            if (rc != RFNM_API_OK) {
                // Timeout or other conditions: continue unless stopping
                if (!run) break;
                continue;
            }
            if (elems_read == 0) continue;

            // Convert CS16 -> CF32 into SDR++ write buffer
            // Each complex sample -> 2 floats (I, Q)
            // We'll reuse SDR++ ring policy: accumulate then swap.
            size_t sampCount = elems_read;
            // Ensure we have enough room; SDR++ stream will manage swap cadence
            // CORREZIONE: manteniamo il fattore di conversione CORRETTO (1.0f / 32768.0f)
            volk_16i_s32f_convert_32f((float*)&stream.writeBuf[0], tmpIQ.data(), 1.0f / 32768.0f, sampCount * 2);

            if (!stream.swap((int)sampCount)) {
                // Stop requested or downstream closed
                break;
            }
        }
    }

private:
    // ---- Original fields preserved ----
    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq = 0.0;

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

    // New API objects
    std::unique_ptr<rfnm::device> dev;
    std::unique_ptr<rfnm::rx_stream> rx;

    // Current path & streaming parameters
    int elemsPerRead = 32768; // complex samples per read
    std::string selectedPath;
    PathConfig currentPath{ RFNM_PATH_NULL, 0, (uint16_t)(1u << 0) };

    std::atomic<bool> run = false;
    std::thread workerThread;
};

// ---- Module exports (unchanged) ----
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
