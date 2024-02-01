#include <utils/net.h>
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <utils/optionlist.h>
#include <algorithm>
#include <dsp/sink/handler_sink.h>
#include <volk/volk.h>
#include <signal_path/signal_path.h>
#include <core.h>

SDRPP_MOD_INFO{
    /* Name:            */ "iq_exporter",
    /* Description:     */ "Export raw IQ through TCP or UDP",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

ConfigManager config;

enum Mode {
    MODE_NONE = -1,
    MODE_BASEBAND,
    MODE_VFO
};

enum Protocol {
    PROTOCOL_TCP,
    PROTOCOL_UDP
};

enum SampleType {
    SAMPLE_TYPE_INT8,
    SAMPLE_TYPE_INT16,
    SAMPLE_TYPE_INT32,
    SAMPLE_TYPE_FLOAT32
};

class IQExporterModule : public ModuleManager::Instance {
public:
    IQExporterModule(std::string name) {
        this->name = name;

        // Define operating modes
        modes.define("Baseband", MODE_BASEBAND);
        modes.define("VFO", MODE_VFO);

        // Define VFO samplerates
        for (int i = 3000; i <= 192000; i <<= 1) {
            samplerates.define(i, getSrScaled(i), i);
        }
        for (int i = 250000; i < 1000000; i += 250000) {
            samplerates.define(i, getSrScaled(i), i);
        }
        for (int i = 1000000; i < 10000000; i += 500000) {
            samplerates.define(i, getSrScaled(i), i);
        }
        for (int i = 10000000; i <= 100000000; i += 5000000) {
            samplerates.define(i, getSrScaled(i), i);
        }

        // Define protocols
        protocols.define("TCP", PROTOCOL_TCP);
        protocols.define("UDP", PROTOCOL_UDP);

        // Define sample types
        sampleTypes.define("Int8", SAMPLE_TYPE_INT8);
        sampleTypes.define("Int16", SAMPLE_TYPE_INT16);
        sampleTypes.define("Int32", SAMPLE_TYPE_INT32);
        sampleTypes.define("Float32", SAMPLE_TYPE_FLOAT32);

        // Load config
        bool autoStart = false;
        Mode nMode = MODE_BASEBAND;
        config.acquire();
        if (config.conf[name].contains("mode")) {
            std::string modeStr = config.conf[name]["mode"];
            if (modes.keyExists(modeStr)) { nMode = modes.value(modes.keyId(modeStr)); }
        }
        if (config.conf[name].contains("samplerate")) {
            int sr = config.conf[name]["samplerate"];
            if (samplerates.keyExists(sr)) { samplerate = samplerates.value(samplerates.keyId(sr)); }
        }
        if (config.conf[name].contains("protocol")) {
            std::string protoStr = config.conf[name]["protocol"];
            if (protocols.keyExists(protoStr)) { proto = protocols.value(protocols.keyId(protoStr)); }
        }
        if (config.conf[name].contains("sampleType")) {
            std::string sampTypeStr = config.conf[name]["sampleType"];
            if (sampleTypes.keyExists(sampTypeStr)) { sampType = sampleTypes.value(sampleTypes.keyId(sampTypeStr)); }
        }
        if (config.conf[name].contains("host")) {
            std::string hostStr = config.conf[name]["host"];
            strcpy(hostname, hostStr.c_str());
        }
        if (config.conf[name].contains("port")) {
            port = config.conf[name]["port"];
            port = std::clamp<int>(port, 1, 65535);
        }
        if (config.conf[name].contains("running")) {
            autoStart = config.conf[name]["running"];
        }
        config.release();

        // Set menu IDs
        modeId = modes.valueId(nMode);
        srId = samplerates.valueId(samplerate);
        protoId = protocols.valueId(proto);
        sampTypeId = sampleTypes.valueId(sampType);

        // Allocate buffer
        buffer = dsp::buffer::alloc<uint8_t>(STREAM_BUFFER_SIZE * sizeof(dsp::complex_t));

        // Init DSP
        handler.init(&iqStream, dataHandler, this);

        // Set operating mode
        setMode(nMode);

        // Start if needed
        if (autoStart) { start(); }

        // Register menu entry
        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~IQExporterModule() {
        // Un-register menu entry
        gui::menu.removeEntry(name);

        // Stop networking
        stop();

        // Stop DSP
        setMode(MODE_NONE);

        // Free buffer
        dsp::buffer::free(buffer);
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

    void start() {
        if (running) { return; }

        // Acquire lock on the socket
        std::lock_guard lck1(sockMtx);

        // Start listening or open UDP socket
        try {
            if (proto == PROTOCOL_TCP) {
                // Create listener
                listener = net::listen(hostname, port);

                // Start listen worker
                listenWorkerThread = std::thread(&IQExporterModule::listenWorker, this);
            }
            else {
                // Open UDP socket
                sock = net::openudp(hostname, port, "0.0.0.0", 0, true);
            }
        }
        catch (const std::exception& e) {
            flog::error("[IQExporter] Could not start socket: {}", e.what());
        }

        running = true;
    }

    void stop() {
        if (!running) { return; }

        // Acquire lock on the socket
        std::lock_guard lck1(sockMtx);

        // Stop listening or close UDP socket
        if (proto == PROTOCOL_TCP) {
            // Stop listener
            if (listener) {
                listener->stop();
            }

            // Wait for worker to stop
            if (listenWorkerThread.joinable()) { listenWorkerThread.join(); }

            // Free listener
            listener.reset();

            // Close socket and free it
            if (sock) {
                sock->close();
                sock.reset();
            }
        }
        else {
            // Close UDP socket and free it
            if (sock) {
                sock->close();
                sock.reset();
            }
        }

        running = false;
    }

private:
    std::string getSrScaled(double sr) {
        char buf[1024];
        if (sr >= 1000000.0) {
            sprintf(buf, "%.1lf MS/s", sr / 1000000.0);
        }
        else if (sr >= 1000.0) {
            sprintf(buf, "%.1lf KS/s", sr / 1000.0);
        }
        else {
            sprintf(buf, "%.1lf S/s", sr);
        }
        return std::string(buf);
    }

    static void menuHandler(void* ctx) {
        IQExporterModule* _this = (IQExporterModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;
        
        if (!_this->enabled) { ImGui::BeginDisabled(); }
        
        if (_this->running) { ImGui::BeginDisabled(); }

        // Mode selector
        ImGui::LeftLabel("Mode");
        ImGui::FillWidth();
        if (ImGui::Combo(("##iq_exporter_mode_" + _this->name).c_str(), &_this->modeId, _this->modes.txt)) {
            _this->setMode(_this->modes.value(_this->modeId));
            config.acquire();
            config.conf[_this->name]["mode"] = _this->modes.key(_this->modeId);
            config.release(true);
        }

        // In VFO mode, show samplerate selector
        if (_this->mode == MODE_VFO) {
            ImGui::LeftLabel("Samplerate");
            ImGui::FillWidth();
            if (ImGui::Combo(("##iq_exporter_sr_" + _this->name).c_str(), &_this->srId, _this->samplerates.txt)) {
                _this->samplerate = _this->samplerates.value(_this->srId);
                if (_this->vfo) {
                    _this->vfo->setBandwidthLimits(_this->samplerate, _this->samplerate, true);
                    _this->vfo->setSampleRate(_this->samplerate, _this->samplerate);
                }
                config.acquire();
                config.conf[_this->name]["samplerate"] = _this->samplerates.key(_this->srId);
                config.release(true);
            }
        }

        // Mode protocol selector
        ImGui::LeftLabel("Protocol");
        ImGui::FillWidth();
        if (ImGui::Combo(("##iq_exporter_proto_" + _this->name).c_str(), &_this->protoId, _this->protocols.txt)) {
            _this->proto = _this->protocols.value(_this->protoId);
            config.acquire();
            config.conf[_this->name]["protocol"] = _this->protocols.key(_this->protoId);
            config.release(true);
        }

        // Sample type selector
        ImGui::LeftLabel("Sample type");
        ImGui::FillWidth();
        if (ImGui::Combo(("##iq_exporter_samp_" + _this->name).c_str(), &_this->sampTypeId, _this->sampleTypes.txt)) {
            _this->sampType = _this->sampleTypes.value(_this->sampTypeId);
            config.acquire();
            config.conf[_this->name]["sampleType"] = _this->sampleTypes.key(_this->sampTypeId);
            config.release(true);
        }

        // Hostname and port field
        if (ImGui::InputText(("##iq_exporter_host_" + _this->name).c_str(), _this->hostname, sizeof(_this->hostname))) {
            config.acquire();
            config.conf[_this->name]["host"] = _this->hostname;
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::FillWidth();
        if (ImGui::InputInt(("##iq_exporter_port_" + _this->name).c_str(), &_this->port, 0, 0)) {
            _this->port = std::clamp<int>(_this->port, 1, 65535);
            config.acquire();
            config.conf[_this->name]["port"] = _this->port;
            config.release(true);
        }

        if (_this->running) { ImGui::EndDisabled(); }

        // Start/Stop buttons
        if (_this->running) {
            if (ImGui::Button(("Stop##iq_exporter_stop_" + _this->name).c_str(), ImVec2(menuWidth, 0))) {
                _this->stop();
                config.acquire();
                config.conf[_this->name]["running"] = false;
                config.release(true);
            }
        }
        else {
            if (ImGui::Button(("Start##iq_exporter_start_" + _this->name).c_str(), ImVec2(menuWidth, 0))) {
                _this->start();
                config.acquire();
                config.conf[_this->name]["running"] = true;
                config.release(true);
            }
        }

        // Status text
        ImGui::TextUnformatted("Status:");
        ImGui::SameLine();
        if (_this->sock && _this->sock->isOpen()) {
            ImGui::TextColored(ImVec4(0.0, 1.0, 0.0, 1.0), (_this->proto == PROTOCOL_TCP) ? "Connected" : "Sending");
        }
        else if (_this->listener && _this->listener->listening()) {
            ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "Listening");
        }
        else {
            ImGui::TextUnformatted("Idle");
        }

        if (!_this->enabled) { ImGui::EndDisabled(); }
    }

    void setMode(Mode newMode) {
        // If there is no mode to change, do nothing
        flog::debug("Mode change");
        if (mode == newMode) {
            flog::debug("New mode same as existing mode, doing nothing");
            return;
        }

        // Stop the DSP
        flog::debug("Stopping DSP");
        handler.stop();

        // Delete VFO or unbind IQ stream
        if (vfo) {
            flog::debug("Deleting old VFO");
            sigpath::vfoManager.deleteVFO(vfo);
            vfo = NULL;
        }
        if (mode == MODE_BASEBAND) {
            flog::debug("Unbinding old stream");
            sigpath::iqFrontEnd.unbindIQStream(&iqStream);
        }

        // If the mode was none, we're done
        if (newMode == MODE_NONE) {
            flog::debug("Exiting, new mode is NONE");
            return;
        }

        // Create VFO or bind IQ stream
        if (newMode == MODE_VFO) {
            flog::debug("Creating new VFO");
            // Create VFO
            vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, samplerate, samplerate, samplerate, samplerate, true);

            // Set its output as the input to the DSP
            handler.setInput(vfo->output);
        }
        else {
            flog::debug("Binding IQ stream");
            // Bind IQ stream
            sigpath::iqFrontEnd.bindIQStream(&iqStream);

            // Set its output as the input to the DSP
            handler.setInput(&iqStream);
        }

        // Start DSP
        flog::debug("Starting DSP");
        handler.start();

        // Update mode
        flog::debug("Updating mode");
        mode = newMode;
        modeId = modes.valueId(newMode);
    }

    void listenWorker() {
        while (true) {
            // Accept a client
            auto newSock = listener->accept();
            if (!newSock) { break; }

            // Update socket
            {
                std::lock_guard lck(sockMtx);
                sock = newSock;
            }

            // Wait until disconnection
            // TODO
        }
    }

    static void dataHandler(dsp::complex_t* data, int count, void* ctx) {
        IQExporterModule* _this = (IQExporterModule*)ctx;

        // Acquire lock on socket
        std::lock_guard lck(_this->sockMtx);

        // If not valid or open, give uo
        if (!_this->sock || !_this->sock->isOpen()) { return; }

        // Convert the samples or send directory for float32
        int size;
        switch (_this->sampType) {
        case SAMPLE_TYPE_INT8:
            volk_32f_s32f_convert_8i((int8_t*)_this->buffer, (float*)data, 128.0f, count*2);
            size = sizeof(int8_t)*2;
            break;
        case SAMPLE_TYPE_INT16:
            volk_32fc_convert_16ic((lv_16sc_t*)_this->buffer, (lv_32fc_t*)data, count);
            size = sizeof(int16_t)*2;
            break;
        case SAMPLE_TYPE_INT32:
            volk_32f_s32f_convert_32i((int32_t*)_this->buffer, (float*)data, (float)2147483647.0f, count*2);
            size = sizeof(int32_t)*2;
            break;
        case SAMPLE_TYPE_FLOAT32:
            _this->sock->send((uint8_t*)data, count*sizeof(dsp::complex_t));
        default:
            return;
        }

        // Send converted samples
        _this->sock->send(_this->buffer, count*size);
    }

    std::string name;
    bool enabled = true;

    Mode mode = MODE_NONE;
    int modeId;
    int samplerate = 1000000.0;
    int srId;
    Protocol proto = PROTOCOL_TCP;
    int protoId;
    SampleType sampType = SAMPLE_TYPE_INT16;
    int sampTypeId;
    char hostname[1024] = "localhost";
    int port = 1234;
    bool running = false;

    OptionList<std::string, Mode> modes;
    OptionList<int, int> samplerates;
    OptionList<std::string, Protocol> protocols;
    OptionList<std::string, SampleType> sampleTypes;

    VFOManager::VFO* vfo = NULL;
    dsp::stream<dsp::complex_t> iqStream;
    dsp::sink::Handler<dsp::complex_t> handler;
    uint8_t* buffer = NULL;

    std::thread listenWorkerThread;

    std::mutex sockMtx;
    std::shared_ptr<net::Socket> sock;
    std::shared_ptr<net::Listener> listener;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    std::string root = (std::string)core::args["root"];
    config.setPath(root + "/iq_exporter_config_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new IQExporterModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (IQExporterModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}