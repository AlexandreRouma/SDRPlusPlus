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
        config.acquire();
        if (config.conf[name].contains("mode")) {
            std::string modeStr = config.conf[name]["mode"];
            if (modes.keyExists(modeStr)) { mode = modes.value(modes.keyId(modeStr)); }
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
        modeId = modes.valueId(mode);
        protoId = protocols.valueId(proto);
        sampTypeId = sampleTypes.valueId(sampType);

        // Allocate buffer
        buffer = dsp::buffer::alloc<uint8_t>(STREAM_BUFFER_SIZE * sizeof(dsp::complex_t));

        // Init DSP
        handler.init(NULL, dataHandler, this);

        // Register menu entry
        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~IQExporterModule() {
        // Un-register menu entry
        gui::menu.removeEntry(name);

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

        // TODO

        running = true;
    }

    void stop() {
        if (!running) { return; }

        // TODO

        running = false;
    }

private:
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

        // Mode protocol selector
        ImGui::LeftLabel("Protocol");
        ImGui::FillWidth();
        if (ImGui::Combo(("##iq_exporter_proto_" + _this->name).c_str(), &_this->protoId, _this->protocols.txt)) {
            config.acquire();
            config.conf[_this->name]["protocol"] = _this->protocols.key(_this->protoId);
            config.release(true);
        }

        // Sample type selector
        ImGui::LeftLabel("Sample type");
        ImGui::FillWidth();
        if (ImGui::Combo(("##iq_exporter_samp_" + _this->name).c_str(), &_this->sampTypeId, _this->sampleTypes.txt)) {
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

        if (!_this->enabled) { ImGui::EndDisabled(); }
    }

    void setMode(Mode newMode) {
        // Delete VFO or unbind IQ stream

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

    Mode mode = MODE_BASEBAND;
    int modeId;
    Protocol proto = PROTOCOL_TCP;
    int protoId;
    SampleType sampType = SAMPLE_TYPE_INT16;
    int sampTypeId;
    char hostname[1024] = "localhost";
    int port = 1234;
    bool running = false;

    OptionList<std::string, Mode> modes;
    OptionList<std::string, Protocol> protocols;
    OptionList<std::string, SampleType> sampleTypes;

    VFOManager::VFO* vfo = NULL;
    dsp::sink::Handler<dsp::complex_t> handler;
    uint8_t* buffer = NULL;

    std::mutex sockMtx;
    std::shared_ptr<net::Socket> sock;
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