#include <string>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Modules.hpp>
#include <dsp/stream.h>
#include <dsp/types.h>

namespace io {
    class SoapyWrapper {
    public:
        SoapyWrapper() {
            output.init(64000);
            refresh();
            setDevice(devList[0]);
        }

        void start() {
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
            for (int i = 0; i < devList.size(); i++) {
                txtDevList += devList[i]["label"];
                txtDevList += '\0';
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
        }

        void setSampleRate(float sampleRate) {
            if (running) {
                return;
            }
            _sampleRate = sampleRate;
            dev->setSampleRate(SOAPY_SDR_RX, 0, sampleRate);
        }

        void setFrequency(float freq) {
            dev->setFrequency(SOAPY_SDR_RX, 0, freq);
        }

        bool isRunning() {
            return running;
        }

        SoapySDR::KwargsList devList;
        std::string txtDevList;
        std::vector<double> sampleRates;
        std::string txtSampleRateList;

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
            printf("Read worker terminated\n");
            delete[] buf;
        }

        SoapySDR::Kwargs args;
        SoapySDR::Device* dev;
        SoapySDR::Stream* _stream;
        std::thread _workerThread;
        bool running = false;
        float _sampleRate = 0;
    };
};