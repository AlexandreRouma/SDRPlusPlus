#pragma once
#include <utils/net.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <thread>

namespace rtltcp {
#pragma pack(push, 1)
        struct Command {
            uint8_t cmd;
            uint32_t param;
        };
#pragma pack(pop)

    class Client {
    public:
        Client(std::shared_ptr<net::Socket> sock, dsp::stream<dsp::complex_t>* stream);
        ~Client();

        bool isOpen();
        void close();

        void setFrequency(double freq);
        void setSampleRate(double sr);
        void setGainMode(int mode);
        void setGain(double gain);
        void setPPM(int ppm);
        void setAGCMode(int mode);
        void setDirectSampling(int mode);
        void setOffsetTuning(bool enabled);
        void setGainIndex(int index);
        void setBiasTee(bool enabled);

    private:
        void sendCommand(uint8_t command, uint32_t param);
        void worker();

        std::shared_ptr<net::Socket> sock;
        std::thread workerThread;
        dsp::stream<dsp::complex_t>* stream;
        int bufferSize = 2400000 / 200;
    };

    std::shared_ptr<Client> connect(dsp::stream<dsp::complex_t>* stream, std::string host, int port = 1234);
}
