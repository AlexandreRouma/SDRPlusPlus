#include <utils/networking.h>
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <dsp/audio.h>
#include <dsp/processing.h>
#include <spdlog/spdlog.h>
#include <config.h>
#include <options.h>
#include <gui/style.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "network_sink",
    /* Description:     */ "Network sink module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

enum {
    SINK_MODE_TCP,
    SINK_MODE_UDP
};

const char* sinkModesTxt = "TCP\0UDP\0";

class NetworkSink : SinkManager::Sink {
public:
    NetworkSink(SinkManager::Stream* stream, std::string streamName) {
        _stream = stream;
        _streamName = streamName;

        // Load config
        config.acquire();
        if (!config.conf.contains(_streamName)) {
            config.conf[_streamName]["hostname"] = "localhost";
            config.conf[_streamName]["port"] = 7355;
            config.conf[_streamName]["protocol"] = SINK_MODE_UDP; // UDP
            config.conf[_streamName]["sampleRate"] = 48000.0;
            config.conf[_streamName]["stereo"] = false;
            config.conf[_streamName]["listening"] = false;
        }
        std::string host = config.conf[_streamName]["hostname"];
        strcpy(hostname, host.c_str());
        port = config.conf[_streamName]["port"];
        modeId = config.conf[_streamName]["protocol"];
        sampleRate = config.conf[_streamName]["sampleRate"];
        stereo = config.conf[_streamName]["stereo"];
        bool startNow = config.conf[_streamName]["listening"];
        config.release(true);

        netBuf = new int16_t[STREAM_BUFFER_SIZE];

        packer.init(_stream->sinkOut, 512);
        s2m.init(&packer.out);
        monoSink.init(&s2m.out, monoHandler, this);
        stereoSink.init(&packer.out, stereoHandler, this);


        // Create a list of sample rates
        for (int sr = 12000; sr < 200000; sr += 12000) {
            sampleRates.push_back(sr);
        }
        for (int sr = 11025; sr < 192000; sr += 11025) {
            sampleRates.push_back(sr);
        }

        // Sort sample rate list
        std::sort(sampleRates.begin(), sampleRates.end(), [](double a, double b) { return (a < b); });

        // Generate text list for UI
        char buffer[128];
        int id = 0;
        int _48kId;
        bool found = false;
        for (auto sr : sampleRates) {
            sprintf(buffer, "%d", (int)sr);
            sampleRatesTxt += buffer;
            sampleRatesTxt += '\0';
            if (sr == sampleRate) {
                srId = id;
                found = true;
            }
            if (sr == 48000.0) { _48kId = id; }
            id++;
        }
        if (!found) {
            srId = _48kId;
            sampleRate = 48000.0;
        }
        _stream->setSampleRate(sampleRate);

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

    void menuHandler() {
        float menuWidth = ImGui::GetContentRegionAvail().x;

        bool listening = (listener && listener->isListening()) || (conn && conn->isOpen());

        if (listening) { style::beginDisabled(); }
        if (ImGui::InputText(CONCAT("##_network_sink_host_", _streamName), hostname, 1023)) {
            config.acquire();
            config.conf[_streamName]["hostname"] = hostname;
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##_network_sink_port_", _streamName), &port, 0, 0)) {
            config.acquire();
            config.conf[_streamName]["port"] = port;
            config.release(true);
        }

        ImGui::LeftLabel("Protocol");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_network_sink_mode_", _streamName), &modeId, sinkModesTxt)) {
            config.acquire();
            config.conf[_streamName]["protocol"] = modeId;
            config.release(true);
        }

        if (listening) { style::endDisabled(); }

        ImGui::LeftLabel("Samplerate");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_network_sink_sr_", _streamName), &srId, sampleRatesTxt.c_str())) {
            sampleRate = sampleRates[srId];
            _stream->setSampleRate(sampleRate);
            packer.setSampleCount(sampleRate / 60);
            config.acquire();
            config.conf[_streamName]["sampleRate"] = sampleRate;
            config.release(true);
        }

        if (ImGui::Checkbox(CONCAT("Stereo##_network_sink_stereo_", _streamName), &stereo)) {
            stop();
            start();
            config.acquire();
            config.conf[_streamName]["stereo"] = stereo;
            config.release(true);
        }

        if (listening && ImGui::Button(CONCAT("Stop##_network_sink_stop_", _streamName), ImVec2(menuWidth, 0))) {
            stopServer();
            config.acquire();
            config.conf[_streamName]["listening"] = false;
            config.release(true);
        }
        else if (!listening && ImGui::Button(CONCAT("Start##_network_sink_stop_", _streamName), ImVec2(menuWidth, 0))) {
            startServer();
            config.acquire();
            config.conf[_streamName]["listening"] = true;
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
            spdlog::warn("Starting");
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

    SinkManager::Stream* _stream;
    dsp::Packer<dsp::stereo_t> packer;
    dsp::StereoToMono s2m;
    dsp::HandlerSink<float> monoSink;
    dsp::HandlerSink<dsp::stereo_t> stereoSink;

    std::string _streamName;

    int srId = 0;
    bool running = false;

    char hostname[1024];
    int port = 4242;

    int modeId = 1;

    std::vector<unsigned int> sampleRates;
    std::string sampleRatesTxt;
    unsigned int sampleRate = 48000;
    bool stereo = false;

    int16_t* netBuf;

    net::Listener listener;
    net::Conn conn;
    std::mutex connMtx;
};

class NetworkSinkModule : public ModuleManager::Instance {
public:
    NetworkSinkModule(std::string name) {
        this->name = name;
        provider.create = create_sink;
        provider.ctx = this;

        sigpath::sinkManager.registerSinkProvider("Network", provider);
    }

    ~NetworkSinkModule() {
        // Unregister sink, this will automatically stop and delete all instances of the audio sink
        sigpath::sinkManager.unregisterSinkProvider("Network");
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
    static SinkManager::Sink* create_sink(SinkManager::Stream* stream, std::string streamName, void* ctx) {
        return (SinkManager::Sink*)(new NetworkSink(stream, streamName));
    }

    std::string name;
    bool enabled = true;
    SinkManager::SinkProvider provider;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(options::opts.root + "/network_sink_config.json");
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