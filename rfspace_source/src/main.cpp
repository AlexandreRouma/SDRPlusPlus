#include <rfspace_client.h>
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
    /* Name:            */ "rfspace_source",
    /* Description:     */ "RFSpace source module for SDR++",
    /* Author:          */ "N2CR",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

const double sampleRates[] = {
    32000,
    48000,
    60000,
    80000,
    96000,
    120000,
    128000,
    160000,
    192000,
    240000,
    256000,
    320000,
    384000,
    480000,
    512000,
    640000,
    768000,
    960000,
    1024000,
    1280000,
    1807058.8,
    2048000
};

const char* sampleRatesTxt[] = {
    "32 kHz",
    "48 kHz",
    "60 kHz",
    "80 kHz",
    "96 kHz",
    "120 kHz",
    "128 kHz",
    "160 kHz",
    "192 kHz",
    "240 kHz",
    "256 kHz",
    "320 kHz",
    "384 kHz",
    "480 kHz",
    "512 kHz",
    "640 kHz",
    "768 kHz",
    "960 kHz",
    "1.024 MHz",
    "1.28 MHz",
    "1.807058 MHz",
    "2.048 MHz"
};

const uint8_t rfGains[] = {
    RFSpaceClient::RF_GAIN_0_DB,
    RFSpaceClient::RF_GAIN_MINUS_10_DB,
    RFSpaceClient::RF_GAIN_MINUS_20_DB,
    RFSpaceClient::RF_GAIN_MINUS_30_DB
};

const char* rfGainsTxt[] = {
    "0 dB",
    "-10 dB",
    "-20 dB",
    "-30 dB",
};

#define SAMPLE_RATE_COUNT  (sizeof(sampleRates) / sizeof(double))

class RFSPACESourceModule : public ModuleManager::Instance {
public:
    RFSPACESourceModule(std::string name) {
        this->name = name;

        sampleRate = 1807058.8;
        freq = 105500000.0;

        int _24id = 0;
        for (int i = 0; i < SAMPLE_RATE_COUNT; i++) {
            if (sampleRates[i] == 2048000.0) { _24id = i; };
            srTxt += sampleRatesTxt[i];
            srTxt += '\0';
        }
        srId = 20;

        config.acquire();
        std::string hostStr = config.conf["host"];
        port = config.conf["port"];
        double wantedSr = config.conf["sampleRate"];
        //gain = std::clamp<int>(config.conf["gainIndex"], 0, 4);
        hostStr = hostStr.substr(0, 1023);
        strcpy(ip, hostStr.c_str());
        config.release();

        bool found = false;
        for (int i = 0; i < SAMPLE_RATE_COUNT; i++) {
            if (sampleRates[i] == wantedSr) {
                found = true;
                srId = i;
                sampleRate = sampleRates[i];
                break;
            }
        }

        if (!found) {
            srId = _24id;
            sampleRate = sampleRates[_24id];
        }

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &output;
        sigpath::sourceManager.registerSource("RFSPACE", &handler);
    }

    ~RFSPACESourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("RFSPACE");
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
    static void menuSelected(void* ctx) {
        RFSPACESourceModule* _this = (RFSPACESourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("RFSPACESourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        RFSPACESourceModule* _this = (RFSPACESourceModule*)ctx;
        spdlog::info("RFSPACESourceModule '{0}': Menu Deselect!", _this->name);
    }
    
    static void start(void* ctx) {
        RFSPACESourceModule* _this = (RFSPACESourceModule*)ctx;
        if (_this->running) { return; }
        if (!_this->client.connectToRFSpace(_this->ip, _this->port)) {
            spdlog::error("Could not connect to {0}:{1}", _this->ip, _this->port);
            return;
        }
        spdlog::warn("Setting sample rate to {0}", _this->sampleRate);

        uint8_t resp[256] = { 0 };

        // initialize radio - uses same sequence as SDR-Radio
        _this->client.getRadioName();
        _this->client.getRadioSerialNum();
        _this->client.getInterfaceVersion();
        _this->client.getVersion(_this->client.BOOT_CODE_VER);
        _this->client.getVersion(_this->client.LOADED_FW_VER);
        _this->client.getVersion(_this->client.HARDWARE_VER);
        _this->client.getStatusErrCode();
        _this->client.getProductId();
        _this->client.getOptions();
        _this->client.getSecurityCode();
        _this->client.setSampleRate(_this->sampleRate);
        _this->client.setRfFilterSelection(_this->client.FLT_AUTO_SELECT);
        _this->client.setRfGain(_this->client.RF_GAIN_0_DB);
        _this->client.setAtoDMode(_this->client.AD_GAIN_1_DOT_5);
        _this->client.setFrequency(_this->freq);
        _this->client.setRfGain(_this->client.RF_GAIN_MINUS_20_DB); // FIXME: convert value from config
        _this->client.setRFInput(_this->client.RF_AUTO_SELECT);
        _this->client.setStart();
        _this->client.getStatusErrCode();

        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        spdlog::info("RFSPACESourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        RFSPACESourceModule* _this = (RFSPACESourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->client.setStop();
        _this->output.stopWriter();
        _this->workerThread.join();
        _this->output.clearWriteStop();
        _this->client.disconnect();
        spdlog::info("RFSPACESourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        RFSPACESourceModule* _this = (RFSPACESourceModule*)ctx;
        if (_this->running) {
            _this->client.setFrequency(freq);
        }
        _this->freq = freq;
        spdlog::info("RFSPACESourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        RFSPACESourceModule* _this = (RFSPACESourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();
        float portWidth = ImGui::CalcTextSize("00000").x + 20;

        if (_this->running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(menuWidth - portWidth);
        if (ImGui::InputText(CONCAT("##_ip_select_", _this->name), _this->ip, 1024)) {
            config.acquire();
            config.conf["host"] = std::string(_this->ip);
            config.release(true);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(portWidth);
        if (ImGui::InputInt(CONCAT("##_port_select_", _this->name), &_this->port, 0)) {
            config.acquire();
            config.conf["port"] = _this->port;
            config.release(true);
        }

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_rfspace_sr_", _this->name), &_this->srId, _this->srTxt.c_str())) {
            _this->sampleRate = sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { style::endDisabled(); }

        //if (_this->tunerGain) { style::beginDisabled(); }
        //ImGui::SetNextItemWidth(menuWidth);
        //if (ImGui::SliderInt(CONCAT("##_gain_select_", _this->name), &_this->gain, -30, 0, "")) {
        //    if (_this->running) {
        //        _this->client.setGainIndex(_this->gain);
        //    }
        //    config.acquire();
        //    config.conf["gainIndex"] = _this->gain;
        //    config.release(true);
        //}
        //if (_this->tunerGain) { style::endDisabled(); }
    }

    static void workerGoodAudioFreq(void* ctx) {
      RFSPACESourceModule* _this = (RFSPACESourceModule*)ctx;
      size_t blockSize = _this->sampleRate / 200.0f;    // 9035.294 (sr=1807058.8 MHz)
      blockSize = (blockSize + RFSPACE_16BIT_SAMPLES_PER_PKT - 1) & -RFSPACE_16BIT_SAMPLES_PER_PKT; // round up to multiple of samples in a packet

      int16_t* inBuf = new int16_t[blockSize];
      long outIdx = -1;
      size_t bytesrecd = 0;

      while (true)
      {
          _this->client.receiveData((uint8_t*)inBuf, (size_t)blockSize * sizeof(int16_t), bytesrecd);

          for (int i = 0; i < blockSize; i += 2) {  // each sample has both I and Q
            outIdx++;
            RFSpaceClient::RFSpaceIQSample sample = *(RFSpaceClient::RFSpaceIQSample*)&inBuf[i];
            _this->output.writeBuf[outIdx].re = (float)sample.I / 32768.0f;
            _this->output.writeBuf[outIdx].im = (float)sample.Q / 32768.0f;

            if (outIdx >= blockSize) {
              outIdx = -1;
              _this->output.swap(blockSize);
            }
          }
      }

      delete[] inBuf;
    }

    static void worker(void* ctx) {
      RFSPACESourceModule* _this = (RFSPACESourceModule*)ctx;
      size_t blockSize = _this->sampleRate / 200.0f;    // 9035.294 (sr=1807058.8 MHz)
      blockSize = (blockSize + RFSPACE_16BIT_SAMPLES_PER_PKT - 1) & -RFSPACE_16BIT_SAMPLES_PER_PKT; // round up to multiple of samples in a packet

      int16_t* inBuf = new int16_t[blockSize];
      int32_t outIdx = -1;
      size_t bytesrecd = 0;

      while (true)
      {
        _this->client.receiveData((uint8_t*)inBuf, (size_t)blockSize * sizeof(int16_t), bytesrecd);

        if (0 == bytesrecd)
          break;

        for (int i = 0; i < blockSize; i += 2) {  // each sample has both I and Q
          outIdx++;
          RFSpaceClient::RFSpaceIQSample sample = *(RFSpaceClient::RFSpaceIQSample*)&inBuf[i];
          _this->output.writeBuf[outIdx].re = (float)sample.Q / 32768.0f;
          _this->output.writeBuf[outIdx].im = (float)sample.I / 32768.0f;

          if (outIdx >= blockSize) {
            outIdx = -1;
            _this->output.swap(blockSize);
          }
        }
      }

      delete[] inBuf;
    }

    static void hexdump(const char* var, void* pAddressIn, long  lSize)
    {
      char szBuf[512] = { 0 };
      long lIndent = 1;
      long lOutLen, lIndex, lIndex2, lOutLen2;
      long lRelPos;
      struct { char* pData; unsigned long lSize; } buf;
      unsigned char* pTmp, ucTmp;
      unsigned char* pAddress = (unsigned char*)pAddressIn;

      buf.pData = (char*)pAddress;
      buf.lSize = lSize;

      sprintf(szBuf, "%s\r\n\r\n", var);
#ifdef _WIN32
      OutputDebugString(szBuf);
#else
      printf("%s\n", szBuf);
#endif

      while (buf.lSize > 0)
      {
        pTmp = (unsigned char*)buf.pData;
        lOutLen = (int)buf.lSize;
        if (lOutLen > 16)
          lOutLen = 16;

        // create a 64-character formatted output line:
        sprintf(szBuf, " >                            "
          "                      "
          "    %08llX\r\n", pTmp - pAddress);
        lOutLen2 = lOutLen;

        for (lIndex = 1 + lIndent, lIndex2 = 53 - 15 + lIndent, lRelPos = 0;
          lOutLen2;
          lOutLen2--, lIndex += 2, lIndex2++
          )
        {
          ucTmp = *pTmp++;

          sprintf(szBuf + lIndex, "%02X ", (unsigned short)ucTmp);
          if (!isprint(ucTmp))  ucTmp = '.'; // nonprintable char
          szBuf[lIndex2] = ucTmp;

          if (!(++lRelPos & 3))     // extra blank after 4 bytes
          {
            lIndex++; szBuf[lIndex + 2] = ' ';
          }
        }

        if (!(lRelPos & 3)) lIndex--;

        szBuf[lIndex] = '<';
        szBuf[lIndex + 1] = ' ';

#ifdef _WIN32
        OutputDebugString(szBuf);
#else
        printf("%s\n", szBuf);
#endif

        buf.pData += lOutLen;
        buf.lSize -= lOutLen;
      }
    }


    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> output;
    double sampleRate;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    RFSpaceClient client;
    bool running = false;
    double freq;
    char ip[1024] = "localhost";
    int port = 50000;
    int gain = 0;
    int srId = 0;
    int tunerGain = -20;

    std::string srTxt = "";
};

MOD_EXPORT void _INIT_() {
   config.setPath(options::opts.root + "/rfspace_config.json");
   json defConf;
   defConf["host"] = "localhost";
   defConf["port"] = 50000;
   defConf["sampleRate"] = 1807058.8;
   config.load(defConf);
   config.enableAutoSave();

   config.acquire();
   if (!config.conf.contains("sampleRate")) {
       config.conf["sampleRate"] = 1807058.8;
   }
   config.release(true);
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RFSPACESourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (RFSPACESourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
