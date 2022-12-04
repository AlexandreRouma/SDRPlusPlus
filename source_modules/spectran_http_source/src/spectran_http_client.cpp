#include "spectran_http_client.h"
#include <spdlog/spdlog.h>

SpectranHTTPClient::SpectranHTTPClient(std::string host, int port, dsp::stream<dsp::complex_t>* stream) {
    this->stream = stream;

    // Connect to server
    sock = net::connect(host, port);
    http = net::http::Client(sock);

    // Make request
    net::http::RequestHeader rqhdr(net::http::METHOD_GET, "/stream?format=float32", host);
    http.sendRequestHeader(rqhdr);
    net::http::ResponseHeader rshdr;
    http.recvResponseHeader(rshdr, 5000);

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
            spdlog::error("Couldn't read JSON metadata");
            return;
        }

        // Read (and check for) record separator
        uint8_t rs;
        int rslen = sock->recv(&rs, 1, true, 5000);
        if (rslen != 1 || rs != 0x1E) {
            spdlog::error("Missing record separator");
            return;
        }

        // Read data
        uint8_t* buf = (uint8_t*)stream->writeBuf;
        int sampLen = 0;
        for (int i = jlen + 1; i < clen;) {
            int read = sock->recv(&buf[sampLen], clen - i, true);
            if (read <= 0) {
                spdlog::error("Recv failed while reading data");
                return;
            }
            i += read;
            sampLen += read;
        }

        // Swap to stream
        if (streamingEnabled) {
            if (!stream->swap(sampLen / 8)) { return; }
        }
        
        // Read trailing CRLF
        std::string dummy;
        sock->recvline(dummy, 2);
        if (dummy != "\r") {
            spdlog::error("Missing trailing CRLF");
            return;
        }
    }
}