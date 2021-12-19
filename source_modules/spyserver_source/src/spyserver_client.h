#pragma once
#include <utils/networking.h>
#include <spyserver_protocol.h>
#include <dsp/stream.h>
#include <dsp/types.h>

namespace spyserver {
    class SpyServerClientClass {
    public:
        SpyServerClientClass(net::Conn conn, dsp::stream<dsp::complex_t>* out);
        ~SpyServerClientClass();

        bool waitForDevInfo(int timeoutMS);

        void startStream();
        void stopStream();

        void setSetting(uint32_t setting, uint32_t arg);

        void close();
        bool isOpen();

        int computeDigitalGain(int serverBits, int deviceGain, int decimationId);

        SpyServerDeviceInfo devInfo;

    private:
        void sendCommand(uint32_t command, void* data, int len);
        void sendHandshake(std::string appName);

        int readSize(int count, uint8_t* buffer);

        static void dataHandler(int count, uint8_t* buf, void* ctx);

        net::Conn client;

        uint8_t* readBuf;
        uint8_t* writeBuf;

        bool deviceInfoAvailable = false;
        std::mutex deviceInfoMtx;
        std::condition_variable deviceInfoCnd;

        SpyServerMessageHeader receivedHeader;

        dsp::stream<dsp::complex_t>* output;
    };

    typedef std::unique_ptr<SpyServerClientClass> SpyServerClient;

    SpyServerClient connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* out);

}
