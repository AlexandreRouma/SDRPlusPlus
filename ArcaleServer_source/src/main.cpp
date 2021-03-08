#include <ArcaleRFServer.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <options.h>
#include <gui/style.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "ArcaleServer_source",
    /* Description:     */ "ArcaleServer source module for SDR++",
    /* Author:          */ "sdaviet",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

const double BANDWITH[] = {
    100000,
    250000,
    500000,
    1000000,
    2000000,
    5000000,
    10000000
};

const char* BANDWITHTXT[] = {
    "100KHz",
    "200KHz",
    "500KHz",
    "1MHz",
    "2MHz",
    "5MHz",
    "10MHz"
};



class ARCALESERVERSourceModule : public ModuleManager::Instance {
public:
    ARCALESERVERSourceModule(std::string name) {
        this->name = name;

        int srCount = sizeof(BANDWITHTXT) / sizeof(char*);
        for (int i = 0; i < srCount; i++) {
            srTxt += BANDWITHTXT[i];
            srTxt += '\0';
        }
        srId = 2;

        config.aquire();
        std::string hostStr = config.conf["host"];
        port = config.conf["port"];
        bandwith = config.conf["bandwith"];
        channel = config.conf["channel"];
        hostStr = hostStr.substr(0, 1023);
        strcpy(ip, hostStr.c_str());
        config.release();
        
        srId = std::distance(BANDWITH, std::find(std::begin(BANDWITH), std::end(BANDWITH), bandwith));

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("ArcaleServer", &handler);
        spdlog::set_level(spdlog::level::off);

    }

    ~ARCALESERVERSourceModule() {
        
    }

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
    static void menuSelected(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        
        spdlog::info("ArcaleServerSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        spdlog::info("ArcaleServerSourceModule '{0}': Menu Deselect!", _this->name);

    }
    
    static void start(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        if (_this->running) {
            return;
        }
        _this->client = new ArcaleRfTCPServer(_this->channel + 10040, _this->port, _this->timeOut);

        if (_this->client->Connect(_this->ip, _this->freq)) {
            spdlog::error("Could not connect to {0}:{1}", _this->ip, _this->port);
            return;
        }
        //_this->client->sendChannelInfo(_this->freq, _this->bandwith, _this->sampleRate, false);
        _this->IQPairs = _this->client->ConnectToChannel(_this->freq, _this->bandwith, &_this->sampleRate);
        core::setInputSampleRate(_this->sampleRate);
        spdlog::warn("Setting sample rate to {0}", _this->sampleRate);
        spdlog::info("ArcaleServerSourceModule IQPairs : '{0}'", _this->IQPairs);
        if (_this->IQPairs > 0) {
            _this->running = true;
            _this->threadstate = true;
            _this->workerThread = std::thread(worker, _this);
            spdlog::info("ArcaleServerSourceModule '{0}': Start!", _this->name);
        }
    }
    
    static void stop(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        if (!_this->running) {
            return;
        }
        _this->running = false;
        _this->threadstate = false;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();
        _this->client->disconnect();
        delete _this->client;
        _this->client = nullptr;
        spdlog::info("ArcaleServerSourceModule '{0}': Stop!", _this->name);
    }
    
    static void tune(double freq, void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            _this->client->sendChannelInfo(_this->freq, _this->bandwith, &_this->sampleRate, false);
            core::setInputSampleRate(_this->sampleRate);
        }
        spdlog::info("ArcaleServerSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }
    
    static void menuHandler(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();
        float portWidth = ImGui::CalcTextSize("00000").x + 20;
        float bandTextWidth = ImGui::CalcTextSize("Sample Rate").x + 20;

        if (_this->running) { 
            style::beginDisabled(); 
        }

        ImGui::SetNextItemWidth(menuWidth - portWidth);
        ImGui::InputText(CONCAT("##_ip_select_", _this->name), _this->ip, 1024);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(portWidth);
        ImGui::InputInt(CONCAT("##_port_select_", _this->name), &_this->port, 0);
        ImGui::SetNextItemWidth(menuWidth - ImGui::CalcTextSize("Channel Number (1-8)").x - 10);
        if (_this->channel <= 0) _this->channel = 1;
        if (_this->channel > 8) _this->channel = 8;
        ImGui::InputInt(CONCAT("Server Module (1-8)##ChannelNumber", _this->name), &_this->channel, 1, 1);
        ImGui::SetNextItemWidth(menuWidth - bandTextWidth);
        if (ImGui::Combo(CONCAT("Bandwith##_Bandwith_", _this->name), &_this->srId, _this->srTxt.c_str())) {
            _this->bandwith = BANDWITH[_this->srId];

        }
        if (_this->running) {
            style::endDisabled();
        }
        config.aquire();
        config.conf["host"] = _this->ip;
        config.conf["port"] = _this->port;
        config.conf["bandwith"] = _this->bandwith;
        config.conf["channel"] = _this->channel;
        config.release(true);
    }

    static void worker(void* ctx) {
        ARCALESERVERSourceModule* _this = (ARCALESERVERSourceModule*)ctx;
        int i;
        int bytesRead;
        int dataRead;
        int bytesleft;
        char* ptr_i = NULL;
        char* ptr_q = NULL;


        int blockSize = _this->IQPairs*4;
        char* inBuf = new char[blockSize];
        
        while (_this->threadstate) {
            // Read samples here
            dataRead = 0;
            bytesleft = blockSize;
            while (bytesleft > 0) {
                bytesRead = _this->client->receiveSDRPP(&inBuf[dataRead], bytesleft);
                if (bytesRead <= 0) break;
                bytesleft -= bytesRead;
                dataRead += bytesRead;
            }
            if (dataRead!=blockSize) spdlog::info("ArcaleServerSourceModule data_read {0}", dataRead);

            ptr_q = &inBuf[0];
            ptr_i = &inBuf[2];

            for (i = 0; i < (dataRead/4); i++) {
                _this->stream.writeBuf[i].i = ((float)((ptr_i[0] << 8) | (ptr_i[1] & 0xFF))) / 32768.0f;
                _this->stream.writeBuf[i].q = ((float)((ptr_q[0] << 8) | (ptr_q[1] & 0xFF))) / 32768.0f;
                //spdlog::info("'{0}', '{1}'", _this->stream.writeBuf[i].i, _this->stream.writeBuf[i].q);

                ptr_i = ptr_i + 4;
                ptr_q = ptr_q + 4;
            }
            if (!_this->stream.swap(dataRead / 4)) { break; };
            //spdlog::info("ArcaleServerSourceModule data_read : '{0}', '{1}'", dataRead, i);
            
        }
        delete[] inBuf;
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    ArcaleRfTCPServer* client = nullptr;
    std::thread workerThread;
    bool running = false;
    bool threadstate = false;
    double freq;
    char ip[1024] = "localhost";
    int port = 1234;
    int channel = 1;
    double bandwith = 100000;
    int timeOut = 1000;
    int srId = 0;
    double sampleRate;
    std::string srTxt = "";
    uint32_t IQPairs;
    float downFactor = 32768.0f;
};

MOD_EXPORT void _INIT_() {
   config.setPath(options::opts.root + "/ArcaleServer_config.json");
   json defConf;
   defConf["host"] = "localhost";
   defConf["port"] = 1234;
   defConf["bandwith"] = 1000000;
   defConf["channel"] = 1;
   config.load(defConf);
   config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new ARCALESERVERSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (ARCALESERVERSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}



