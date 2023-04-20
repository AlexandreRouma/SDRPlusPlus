#pragma once
#include <dsp/stream.h>
#include <dsp/types.h>
#include <string>
#include <thread>
#include <utils/proto/http.h>
#include <utils/new_event.h>
#include <stdint.h>

class SpectranHTTPClient {
public:
    SpectranHTTPClient(std::string host, int port, dsp::stream<dsp::complex_t>* stream);

    void startWorker();
    void streaming(bool enabled);
    bool isOpen();
    void close();

    void setCenterFrequency(uint64_t freq);

    NewEvent<uint64_t> onCenterFrequencyChanged;
    NewEvent<uint64_t> onSamplerateChanged;

private:
    void worker();

    std::string host;
    int port;

    std::shared_ptr<net::Socket> sock;
    net::http::Client http;
    dsp::stream<dsp::complex_t>* stream;
    std::thread workerThread;

    bool streamingEnabled = false;
    int64_t _centerFreq = 0;
    uint64_t _samplerate = 0;
};