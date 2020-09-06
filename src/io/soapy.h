#include <string>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Logger.hpp>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <spdlog/spdlog.h>

namespace io {
    class SoapyWrapper {
    public:
        SoapyWrapper() {
            SoapySDR::registerLogHandler(_logHandler);
            SoapySDR::Device::make("");

            output.init(64000);
            currentGains = new float[1];
            refresh();
            if (devList.size() == 0) {
                dev = NULL;
                return;
            }
            setDevice(devList[0]);
        }

        void start() {
            if (devList.size() == 0) {
                return;
            }
            if (running) {
                return;
            }
            dev = SoapySDR::Device::make(args);
            _stream = dev->setupStream(SOAPY_SDR_RX, "CF32");
            dev->activateStream(_stream);
            running = true;
            _workerThread = std::thread(_worker, this);
        }

        void stop() {
            if (!running) {
                return;
            }
            running = false;
            dev->deactivateStream(_stream);
            dev->closeStream(_stream);
            _workerThread.join();
            SoapySDR::Device::unmake(dev);
        }

        void refresh() {
            if (running) {
                return;
            }
            
            devList = SoapySDR::Device::enumerate();
            txtDevList = "";
            devNameList.clear();
            if (devList.size() == 0) {
                txtDevList += '\0';
                return;
            }
            for (int i = 0; i < devList.size(); i++) {
                txtDevList += devList[i]["label"];
                txtDevList += '\0';
                devNameList.push_back(devList[i]["label"]);
            }
        }

        void setDevice(SoapySDR::Kwargs devArgs) {
            if (running) {
                return;
            }
            args = devArgs;
            dev = SoapySDR::Device::make(devArgs);
            txtSampleRateList = "";
            sampleRates = dev->listSampleRates(SOAPY_SDR_RX, 0);
            for (int i = 0; i < sampleRates.size(); i++) {
                txtSampleRateList += std::to_string((int)sampleRates[i]);
                txtSampleRateList += '\0';
            }
            _sampleRate = sampleRates[0];

            gainList = dev->listGains(SOAPY_SDR_RX, 0);
            gainRanges.clear();
            if (gainList.size() == 0) {
                return;
            }
            delete[] currentGains;
            currentGains = new float[gainList.size()];
            for (int i = 0; i < gainList.size(); i++) {
                gainRanges.push_back(dev->getGainRange(SOAPY_SDR_RX, 0, gainList[i]));
                SoapySDR::Range rng = dev->getGainRange(SOAPY_SDR_RX, 0, gainList[i]);

                spdlog::info("{0}: {1} -> {2} (Step: {3})", gainList[i], rng.minimum(), rng.maximum(), rng.step());

                currentGains[i] = rng.minimum();
            }
        }

        void setSampleRate(float sampleRate) {
            if (running || dev == NULL) {
                return;
            }
            _sampleRate = sampleRate;
            dev->setSampleRate(SOAPY_SDR_RX, 0, sampleRate);
        }

        void setFrequency(float freq) {
            if (dev == NULL) {
                return;
            }
            dev->setFrequency(SOAPY_SDR_RX, 0, freq);
        }

        void setGain(int gainId, float gain) {
            if (dev == NULL) {
                return;
            }
            currentGains[gainId] = gain;
            dev->setGain(SOAPY_SDR_RX, 0, gainList[gainId], gain);
        }

        bool isRunning() {
            return running;
        }

        float getSampleRate() {
            return _sampleRate;
        }

        SoapySDR::KwargsList devList;
        std::vector<std::string> devNameList;
        std::string txtDevList;

        std::vector<double> sampleRates;
        std::string txtSampleRateList;

        std::vector<std::string> gainList;
        std::vector<SoapySDR::Range> gainRanges;
        float* currentGains;

        dsp::stream<dsp::complex_t> output;

    private:
        static void _worker(SoapyWrapper* _this) {
            int blockSize = _this->_sampleRate / 200.0f;
            dsp::complex_t* buf = new dsp::complex_t[blockSize];
            int flags = 0;
            long long timeMs = 0;
            
            while (_this->running) {
                int res = _this->dev->readStream(_this->_stream, (void**)&buf, blockSize, flags, timeMs);
                if (res < 1) {
                    continue;
                } 
                _this->output.write(buf, res);
            }
            delete[] buf;
        }

        static void _logHandler(const SoapySDRLogLevel lvl, const char* message) {
            if (lvl == SOAPY_SDR_FATAL || lvl == SOAPY_SDR_CRITICAL || lvl == SOAPY_SDR_ERROR) {
                spdlog::error(message);
            }
            else if (lvl == SOAPY_SDR_WARNING) {
                spdlog::warn(message);
            }
            else if (lvl == SOAPY_SDR_NOTICE | SOAPY_SDR_INFO) {
                spdlog::info(message);
            }
        }

        SoapySDR::Kwargs args;
        SoapySDR::Device* dev;
        SoapySDR::Stream* _stream;
        std::thread _workerThread;
        bool running = false;
        float _sampleRate = 0;
    };
};