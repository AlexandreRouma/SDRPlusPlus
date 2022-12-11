#include "redpitaya.h"
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <gui/smgui.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "redpitaya_source",
    /* Description:     */ "Redpitayasource module for SDR++",
    /* Author:          */ "Johnson",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class RedpitayaSourceModule : public ModuleManager::Instance {
public:
    RedpitayaSourceModule(std::string name) {
        this->name = name;

        config.acquire();
        std::string _ip = config.conf["IP"];
        strcpy(ip, _ip.c_str());
        sampleRate = config.conf["sampleRate"];
        config.release();

        // Generate the samplerate list and find srId
        bool found = false;

        for (int i = 0; i <= redpitaya::REDPITAYA_SAMP_RATE_1250KHZ; i++) {
            sampleRates.push_back(redpitaya::sampleRate[i]);
            sampleRatesTxt += getBandwdithScaled(redpitaya::sampleRate[i]);
            sampleRatesTxt += '\0';

            if (static_cast<int>(redpitaya::sampleRate[i]) == sampleRate) {
                found = true;
                srId = i;
            }
        }
        if (!found) {
            srId = redpitaya::REDPITAYA_SAMP_RATE_1250KHZ;
            sampleRate = redpitaya::sampleRate[redpitaya::REDPITAYA_SAMP_RATE_1250KHZ];
        }

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("Redpitaya", &handler);
    }

    ~RedpitayaSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("Redpitaya");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = true;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void redpitaya_send_command(SOCKET socket, uint32_t command) {
        std::stringstream message;

#if defined(_WIN32)
        int total = sizeof(command);
        int size;
        size = ::send(socket, (char*)&command, sizeof(command), 0);
#else
        ssize_t total = sizeof(command);
        ssize_t size;
        size = ::send(socket, &command, sizeof(command), MSG_NOSIGNAL);
#endif

        if (size != total) {
            spdlog::info("Sending command failed: " + std::to_string(command));
        }
    }

    std::string getBandwdithScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            sprintf(buf, "%.2lfMHz", bw / 1000000.0);
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
        RedpitayaSourceModule* _this = (RedpitayaSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("RedpitayaSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        RedpitayaSourceModule* _this = (RedpitayaSourceModule*)ctx;
        spdlog::info("RedpitayaSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        RedpitayaSourceModule* _this = (RedpitayaSourceModule*)ctx;
        uint32_t command;
        struct sockaddr_in addr;
        unsigned short port = 1001;

        if (_this->running) { return; }

        // TODO: INIT CONTEXT HERE
        for (size_t i = 0; i < 2; ++i) {
            if ((redpitaya::_sockets[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                spdlog::error("Redpitaya ould not create TCP socket.");
                return;
            }
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            inet_pton(AF_INET, _this->ip, &addr.sin_addr);
            addr.sin_port = htons(port);

            if (::connect(redpitaya::_sockets[i], (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                spdlog::error("Redpitaya could not connect to " + std::string(_this->ip) + ":" + std::to_string(port) + ".");
                return;
            }

            command = i;
            redpitaya_send_command(redpitaya::_sockets[i], command);
        }

        // Set sample rate
        command = _this->srId;
        command |= 1 << 28;
        redpitaya_send_command(redpitaya::_sockets[0], command);

        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        spdlog::info("RedpitayaSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        RedpitayaSourceModule* _this = (RedpitayaSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();

        // DESTROY CONTEXT HERE
#if defined(_WIN32)
        ::closesocket(redpitaya::_sockets[1]);
        ::closesocket(redpitaya::_sockets[0]);
        WSACleanup();
#else
        ::close(redpitaya::_sockets[1]);
        ::close(redpitaya::_sockets[0]);
#endif

        spdlog::info("RedpitayaSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        RedpitayaSourceModule* _this = (RedpitayaSourceModule*)ctx;
        uint32_t command = 0;

        if (_this->running) {

            if (freq < _this->sampleRate / 2.0 || freq > 6.0e7) return;

            command = static_cast<uint32_t>(floor(freq + 0.5));

            redpitaya_send_command(redpitaya::_sockets[0], command);

            _this->_freq = freq;
        }
        spdlog::info("RedpitayaSourceModule '{0}': Tune: {1}!", _this->name, _this->_freq);
    }

    static void menuHandler(void* ctx) {
        RedpitayaSourceModule* _this = (RedpitayaSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::LeftLabel("IP");
        SmGui::FillWidth();
        if (SmGui::InputText(CONCAT("##_redpitaya_ip_", _this->name), _this->ip, 16)) {
            config.acquire();
            config.conf["IP"] = _this->ip;
            config.release(true);
        }

        SmGui::LeftLabel("Samplerate");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_redpitaya_sr_", _this->name), &_this->srId, _this->sampleRatesTxt.c_str())) {
            _this->sampleRate = _this->sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["sampleRate"] = _this->sampleRate;
            config.release(true);
        }
        if (_this->running) { SmGui::EndDisabled(); }
    }

    static void worker(void* ctx) {
        RedpitayaSourceModule* _this = (RedpitayaSourceModule*)ctx;
        const int blockSize = 8192;
        dsp::complex_t buf[blockSize];
        ssize_t total = blockSize * sizeof(dsp::complex_t);
        while (true) {
            // Read samples here
            ssize_t size = ::recv(redpitaya::_sockets[1], reinterpret_cast<char*>(_this->stream.writeBuf), total, MSG_WAITALL);
            if (size != total)
            {
                spdlog::error("Redpitaya receiving samples failed.");
                continue;
            }
            if (!_this->stream.swap(blockSize)) { break; };
        }
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    bool running = false;
    double _freq;
    int srId = 0;
    char ip[1024] = "192.168.1.100";

    std::vector<double> sampleRates;
    std::string sampleRatesTxt;
};

MOD_EXPORT void _INIT_() {
    json defConf;
    defConf["IP"] = "192.168.1.100";
    defConf["sampleRate"] = 1250000.0f;
    config.setPath(core::args["root"].s() + "/redpitaya_source_config.json");
    config.load(defConf);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RedpitayaSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (RedpitayaSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}