#include <utils/networking.h>
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <signal_path/stream.h>
#include <dsp/buffer/packer.h>
#include <dsp/convert/stereo_to_mono.h>
#include <dsp/sink/handler_sink.h>
#include <utils/flog.h>
#include <config.h>
#include <gui/style.h>
#include <utils/optionlist.h>
#include <core.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "network_sink",
    /* Description:     */ "Network sink module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 2, 0,
    /* Max instances    */ 1
};

ConfigManager config;

enum SinkMode {
    SINK_MODE_TCP,
    SINK_MODE_UDP
};

const char* sinkModesTxt = "TCP\0UDP\0";

class NetworkSink : public Sink {
public:
    NetworkSink(SinkEntry* entry, dsp::stream<dsp::stereo_t>* stream, const std::string& name, SinkID id, const std::string& stringId) :
        Sink(entry, stream, name, id, stringId)
    {
        // Define modes
        modes.define("TCP", SINK_MODE_TCP);
        modes.define("UDP", SINK_MODE_UDP);

        // Create a list of sample rates
        std::vector<int> srList;
        for (int sr = 12000; sr <= 200000; sr += 12000) {
            srList.push_back(sr);
        }
        for (int sr = 11025; sr <= 192000; sr += 11025) {
            srList.push_back(sr);
        }

        // Sort sample rate list
        std::sort(srList.begin(), srList.end(), [](double a, double b) { return (a < b); });

        // Define samplerate options
        for (int sr : srList) {
            char buf[16];
            sprintf(buf, "%d", sr);
            samplerates.define(sr, buf, sr);
        }

        // Allocate buffer
        netBuf = new int16_t[STREAM_BUFFER_SIZE];

        // Init DSP
        packer.init(stream, 512);
        s2m.init(&packer.out);
        monoSink.init(&s2m.out, monoHandler, this);
        stereoSink.init(&packer.out, stereoHandler, this);

        // Load config
        config.acquire();
        bool startNow = false;
        if (config.conf[stringId].contains("hostname")) {
            std::string host = config.conf[stringId]["hostname"];
            strcpy(hostname, host.c_str());
        }
        if (config.conf[stringId].contains("port")) {
            port = config.conf[stringId]["port"];
        }
        if (config.conf[stringId].contains("mode")) {
            std::string modeStr = config.conf[stringId]["mode"];
            if (modes.keyExists(modeStr)) {
                mode = modes.value(modes.keyId(modeStr));
            }
            else {
                mode = SINK_MODE_TCP;
            }
        }
        if (config.conf[stringId].contains("samplerate")) {
            int nSr = config.conf[stringId]["samplerate"];
            if (samplerates.keyExists(nSr)) {
                sampleRate = samplerates.value(samplerates.keyId(nSr));
            }
            else {
                sampleRate = 48000;
            }
        }
        if (config.conf[stringId].contains("stereo")) {
            stereo = config.conf[stringId]["stereo"];
        }
        if (config.conf[stringId].contains("running")) {
            startNow = config.conf[stringId]["running"];
        }
        config.release();

        // Set mode ID
        modeId = modes.valueId(mode);

        // Set samplerate ID
        srId = samplerates.valueId(sampleRate);

        // Start if needed
        if (startNow) { startServer(); }
    }

    ~NetworkSink() {
        stopServer();
        delete[] netBuf;
    }

    void start() {
        if (running) {
            return;
        }
        doStart();
        running = true;
    }

    void stop() {
        if (!running) {
            return;
        }
        doStop();
        running = false;
    }

    void showMenu() {
        float menuWidth = ImGui::GetContentRegionAvail().x;

        bool listening = (listener && listener->isListening()) || (conn && conn->isOpen());

        if (listening) { style::beginDisabled(); }
        if (ImGui::InputText(CONCAT("##_network_sink_host_", stringId), hostname, 1023)) {
            config.acquire();
            config.conf[stringId]["hostname"] = hostname;
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##_network_sink_port_", stringId), &port, 0, 0)) {
            config.acquire();
            config.conf[stringId]["port"] = port;
            config.release(true);
        }

        ImGui::LeftLabel("Protocol");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_network_sink_mode_", stringId), &modeId, sinkModesTxt)) {
            config.acquire();
            config.conf[stringId]["mode"] = modeId;
            config.release(true);
        }

        if (listening) { style::endDisabled(); }

        ImGui::LeftLabel("Samplerate");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_network_sink_sr_", stringId), &srId, samplerates.txt)) {
            sampleRate = samplerates.value(srId);
            entry->setSamplerate(sampleRate);
            packer.setSampleCount(sampleRate / 60);
            config.acquire();
            config.conf[stringId]["samplerate"] = sampleRate;
            config.release(true);
        }

        if (ImGui::Checkbox(CONCAT("Stereo##_network_sink_stereo_", stringId), &stereo)) {
            stop();
            start();
            config.acquire();
            config.conf[stringId]["stereo"] = stereo;
            config.release(true);
        }

        if (listening && ImGui::Button(CONCAT("Stop##_network_sink_stop_", stringId), ImVec2(menuWidth, 0))) {
            stopServer();
            config.acquire();
            config.conf[stringId]["running"] = false;
            config.release(true);
        }
        else if (!listening && ImGui::Button(CONCAT("Start##_network_sink_stop_", stringId), ImVec2(menuWidth, 0))) {
            startServer();
            config.acquire();
            config.conf[stringId]["running"] = true;
            config.release(true);
        }

        ImGui::TextUnformatted("Status:");
        ImGui::SameLine();
        if (conn && conn->isOpen()) {
            ImGui::TextColored(ImVec4(0.0, 1.0, 0.0, 1.0), (modeId == SINK_MODE_TCP) ? "Connected" : "Sending");
        }
        else if (listening) {
            ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "Listening");
        }
        else {
            ImGui::TextUnformatted("Idle");
        }
    }

private:
    void doStart() {
        packer.start();
        if (stereo) {
            stereoSink.start();
        }
        else {
            flog::warn("Starting");
            s2m.start();
            monoSink.start();
        }
    }

    void doStop() {
        packer.stop();
        s2m.stop();
        monoSink.stop();
        stereoSink.stop();
    }

    void startServer() {
        if (modeId == SINK_MODE_TCP) {
            listener = net::listen(hostname, port);
            if (listener) {
                listener->acceptAsync(clientHandler, this);
            }
        }
        else {
            conn = net::openUDP("0.0.0.0", port, hostname, port, false);
        }
    }

    void stopServer() {
        if (conn) { conn->close(); }
        if (listener) { listener->close(); }
    }

    static void monoHandler(float* samples, int count, void* ctx) {
        NetworkSink* _this = (NetworkSink*)ctx;
        std::lock_guard lck(_this->connMtx);
        if (!_this->conn || !_this->conn->isOpen()) { return; }

        volk_32f_s32f_convert_16i(_this->netBuf, (float*)samples, 32768.0f, count);

        _this->conn->write(count * sizeof(int16_t), (uint8_t*)_this->netBuf);
    }

    static void stereoHandler(dsp::stereo_t* samples, int count, void* ctx) {
        NetworkSink* _this = (NetworkSink*)ctx;
        std::lock_guard lck(_this->connMtx);
        if (!_this->conn || !_this->conn->isOpen()) { return; }

        volk_32f_s32f_convert_16i(_this->netBuf, (float*)samples, 32768.0f, count * 2);

        _this->conn->write(count * 2 * sizeof(int16_t), (uint8_t*)_this->netBuf);
    }

    static void clientHandler(net::Conn client, void* ctx) {
        NetworkSink* _this = (NetworkSink*)ctx;

        {
            std::lock_guard lck(_this->connMtx);
            _this->conn = std::move(client);
        }

        if (_this->conn) {
            _this->conn->waitForEnd();
            _this->conn->close();
        }
        else {
        }

        _this->listener->acceptAsync(clientHandler, _this);
    }

    // DSP
    dsp::buffer::Packer<dsp::stereo_t> packer;
    dsp::convert::StereoToMono s2m;
    dsp::sink::Handler<float> monoSink;
    dsp::sink::Handler<dsp::stereo_t> stereoSink;

    OptionList<std::string, SinkMode> modes;
    OptionList<int, double> samplerates;

    char hostname[1024];
    int port = 7355;
    SinkMode mode = SINK_MODE_TCP;
    int modeId;
    int sampleRate = 48000;
    int srId;
    bool stereo = false;
    bool running = false;

    int16_t* netBuf;
    net::Listener listener;
    net::Conn conn;
    std::mutex connMtx;
};

class NetworkSinkModule : public ModuleManager::Instance, SinkProvider {
public:
    NetworkSinkModule(std::string name) {
        // Register self as provider
        sigpath::streamManager.registerSinkProvider("Network", this);
    }

    ~NetworkSinkModule() {
        // Unregister self
        sigpath::streamManager.unregisterSinkProvider(this);
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

    std::unique_ptr<Sink> createSink(SinkEntry* entry, dsp::stream<dsp::stereo_t>* stream, const std::string& name, SinkID id, const std::string& stringId) {
        return std::make_unique<NetworkSink>(entry, stream, name, id, stringId);
    }

private:
    std::string name;
    bool enabled = true;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(core::args["root"].s() + "/network_sink_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    NetworkSinkModule* instance = new NetworkSinkModule(name);
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (NetworkSinkModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}