#include "spectran_http_client.h"
#include <utils/flog.h>
#include <inttypes.h>

SpectranHTTPClient::SpectranHTTPClient(std::string host, int port, dsp::stream<dsp::complex_t>* stream) {
    this->stream = stream;

    // Connect to server
    this->host = host;
    this->port = port;
    sock = net::connect(host, port);
    http = net::http::Client(sock);

    // Make request
    net::http::RequestHeader rqhdr(net::http::METHOD_GET, "/stream?format=float32", host);
    http.sendRequestHeader(rqhdr);
    net::http::ResponseHeader rshdr;
    http.recvResponseHeader(rshdr, 5000);

    if (rshdr.getStatusCode() != net::http::STATUS_CODE_OK) {
        flog::error("HTTP request did not return ok: {}", rshdr.getStatusString());
        throw std::runtime_error("HTTP request did not return ok");
    }
}

void SpectranHTTPClient::startWorker() {
    // Start chunk worker
    workerThread = std::thread(&SpectranHTTPClient::worker, this);
}

void SpectranHTTPClient::streaming(bool enabled) {
    streamingEnabled = enabled;
}

bool SpectranHTTPClient::isOpen() {
    return sock->isOpen();
}

void SpectranHTTPClient::close() {
    sock->close();
    stream->stopWriter();
    if (workerThread.joinable()) { workerThread.join(); }
    stream->clearWriteStop();
}

void SpectranHTTPClient::setCenterFrequency(uint64_t freq) {
    // Connect to control endpoint (TODO: Switch to an always connected endpoint)
    auto controlSock = net::connect(host, port);
    auto controlHttp = net::http::Client(controlSock);

    // Make request
    net::http::RequestHeader rqhdr(net::http::METHOD_PUT, "/control", host);
    char buf[1024];
    sprintf(buf, "{\"frequencyCenter\":%" PRIu64 ",\"frequencySpan\":%" PRIu64 ",\"type\":\"capture\"}", freq, _samplerate);
    std::string data = buf;
    char lenBuf[16];
    sprintf(lenBuf, "%" PRIu64, (uint64_t)data.size());
    rqhdr.setField("Content-Length", lenBuf);
    controlHttp.sendRequestHeader(rqhdr);
    controlSock->sendstr(data);
    net::http::ResponseHeader rshdr;
    controlHttp.recvResponseHeader(rshdr, 5000);

    flog::debug("Response: {}", rshdr.getStatusString());
}

void SpectranHTTPClient::worker() {
    while (sock->isOpen()) {
        // Get chunk header
        net::http::ChunkHeader chdr;
        int err = http.recvChunkHeader(chdr, 5000);
        if (err < 0) { return; }

        // If null length, finish
        size_t clen = chdr.getLength();
        if (!clen) { return; }

        // Read JSON
        std::string jsonData;
        int jlen = sock->recvline(jsonData, clen, 5000);
        if (jlen <= 0) {
            flog::error("Couldn't read JSON metadata");
            return;
        }

        // Decode JSON (yes, this is hacky, but it must be extremely fast)
        auto startFreqBegin = jsonData.find("\"startFrequency\":");
        auto startFreqEnd = jsonData.find(',', startFreqBegin);
        std::string startFreqStr = jsonData.substr(startFreqBegin + 17, startFreqEnd - startFreqBegin - 17);
        int64_t startFreq = std::stoll(startFreqStr);

        auto endFreqBegin = jsonData.find("\"endFrequency\":");
        auto endFreqEnd = jsonData.find(',', endFreqBegin);
        std::string endFreqStr = jsonData.substr(endFreqBegin + 15, endFreqEnd - endFreqBegin - 15);
        int64_t endFreq = std::stoll(endFreqStr);

        auto sampleFreqBegin = jsonData.find("\"sampleFrequency\":");
        bool sampleFreqReceived = (sampleFreqBegin != -1);
        int64_t sampleFreq;
        if (sampleFreqReceived) {
            auto sampleFreqEnd = jsonData.find(',', sampleFreqBegin);
            std::string sampleFreqStr = jsonData.substr(sampleFreqBegin + 18, sampleFreqEnd - sampleFreqBegin - 18);
            sampleFreq = std::stoll(sampleFreqStr);
            //flog::debug("{}", jsonData);
        }
        
        // Calculate and update center freq
        int64_t samplerate = /* sampleFreqReceived ? sampleFreq :  */(endFreq - startFreq);
        int64_t centerFreq = round(((double)endFreq + (double)startFreq) / 2.0);
        if (centerFreq != _centerFreq) {
            flog::debug("New center freq: {}", centerFreq);
            _centerFreq = centerFreq;
            onCenterFrequencyChanged(centerFreq);
        }
        if (samplerate != _samplerate) {
            flog::debug("New samplerate: {}", samplerate);
            _samplerate = samplerate;
            onSamplerateChanged(samplerate);
        }

        // Read (and check for) record separator
        uint8_t rs;
        int rslen = sock->recv(&rs, 1, true, 5000);
        if (rslen != 1 || rs != 0x1E) {
            flog::error("Missing record separator");
            return;
        }

        // Read data
        uint8_t* buf = (uint8_t*)stream->writeBuf;
        int sampLen = 0;
        for (int i = jlen + 1; i < clen;) {
            int read = sock->recv(&buf[sampLen], clen - i, true);
            if (read <= 0) {
                flog::error("Recv failed while reading data");
                return;
            }
            i += read;
            sampLen += read;
        }
        int sampCount = sampLen / 8;

        // Swap to stream
        if (streamingEnabled) {
            if (!stream->swap(sampCount)) { return; }
        }
        
        // Read trailing CRLF
        std::string dummy;
        sock->recvline(dummy, 2);
        if (dummy != "\r") {
            flog::error("Missing trailing CRLF");
            return;
        }
    }
}