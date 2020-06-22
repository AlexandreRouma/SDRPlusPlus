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
            dsp::complex_t* buf = new dsp::complex_t[32000];
            int flags = 0;
            long long timeMs = 0;
            while (_this->running) {
                _this->dev->readStream(_this->_stream, (void**)&buf, 32000, flags, timeMs);
                _this->output.write(buf, 32000);
            }
            printf("Read worker terminated\n");
            delete[] buf;
        }

        SoapySDR::Kwargs args;
        SoapySDR::Device* dev;
        SoapySDR::Stream* _stream;
        std::thread _workerThread;
        bool running = false;
    };
};