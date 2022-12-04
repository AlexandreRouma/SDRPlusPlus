#pragma once
#include <dsp/stream.h>
#include <dsp/types.h>
#include <string>
#include <thread>
#include "proto/http.h"

class SpectranHTTPClient {
public:
    SpectranHTTPClient(std::string host, int port, dsp::stream<dsp::complex_t>* stream);

    void streaming(bool enabled);
    bool isOpen();
    void close();

private:
    void worker();

    std::shared_ptr<net::Socket> sock;
    net::http::Client http;
    dsp::stream<dsp::complex_t>* stream;
    std::thread workerThread;

    bool streamingEnabled = false;
};