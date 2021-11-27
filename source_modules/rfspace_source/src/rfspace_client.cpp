#include <rfspace_client.h>
#include <volk/volk.h>
#include <cstring>
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

namespace rfspace {
    RFspaceClientClass::RFspaceClientClass(net::Conn conn, net::Conn udpConn, dsp::stream<dsp::complex_t>* out) {
        client = std::move(conn);
        udpClient = std::move(udpConn);
        output = out;
        rbuffer = new uint8_t[RFSPACE_MAX_SIZE];
        sbuffer = new uint8_t[RFSPACE_MAX_SIZE];
        ubuffer = new uint8_t[RFSPACE_MAX_SIZE];

        output->clearWriteStop();

        uint8_t test = 0x5A;
        udpClient->write(1, &test);

        // Start readers
        client->readAsync(sizeof(tcpHeader), (uint8_t*)&tcpHeader, tcpHandler, this);
        udpClient->readAsync(sizeof(udpHeader), (uint8_t*)&udpHeader, udpHandler, this);

        // Start SDR
        uint8_t args[4] = {0x70, 2, 0, 0};
        setControlItem(0x18, args, 4);
    }

    RFspaceClientClass::~RFspaceClientClass() {
        close();
        delete[] rbuffer;
        delete[] sbuffer;
        delete[] ubuffer;
    }

    int RFspaceClientClass::getControlItem(uint16_t item, void* param, int len) {
        // Build packet
        uint16_t* header = (uint16_t*)&sbuffer[0];
        uint16_t* item_val = (uint16_t*)&sbuffer[2];
        *header = 4 | (RFSPACE_MSG_TYPE_H2T_REQ_CTRL_ITEM << 13);
        *item_val = item;

        // Send packet
        SCIRRecv.release();
        client->write(4, sbuffer);

        // Wait for a response
        if (!SCIRRecv.await(2000)) {
            SCIRRecv.release();
            return -1;
        }

        // Forward data
        int toRead = ((SCIRSize - 4) < len) ? (SCIRSize - 4) : len;
        memcpy(param, &rbuffer[4], toRead);

        // Release receiving thread
        SCIRRecv.release();

        return toRead;
    }

    void RFspaceClientClass::setControlItem(uint16_t item, void* param, int len) {
        // Build packet
        uint16_t* header = (uint16_t*)&sbuffer[0];
        uint16_t* item_val = (uint16_t*)&sbuffer[2];
        *header = (len + 4) | (RFSPACE_MSG_TYPE_H2T_SET_CTRL_ITEM << 13);
        *item_val = item;
        memcpy(&sbuffer[4], param, len);

        // Send packet
        client->write(len + 4, sbuffer);
    }

    void RFspaceClientClass::close() {
        output->stopWriter();
        client->close();
        udpClient->close();
    }

    bool RFspaceClientClass::isOpen() {
        return client->isOpen();
    }

    void RFspaceClientClass::tcpHandler(int count, uint8_t* buf, void* ctx) {
        RFspaceClientClass* _this = (RFspaceClientClass*)ctx;
        uint8_t type = _this->tcpHeader >> 13;
        uint16_t size = _this->tcpHeader & 0b1111111111111;

        // Read the rest of the data
        if (size > 2) {
            _this->client->read(size - 2, &_this->rbuffer[2]);
        }

        spdlog::warn("TCP received: {0} {1}", type, size);

        // Process data
        if (type == RFSPACE_MSG_TYPE_T2H_SET_CTRL_ITEM_RESP) {
            _this->SCIRSize = size;
            _this->SCIRRecv.trigger();
        }

        // Restart an async read
        _this->client->readAsync(sizeof(_this->tcpHeader), (uint8_t*)&_this->tcpHeader, tcpHandler, _this);
    }

    void RFspaceClientClass::udpHandler(int count, uint8_t* buf, void* ctx) {
        RFspaceClientClass* _this = (RFspaceClientClass*)ctx;
        uint8_t type = _this->udpHeader >> 13;
        uint16_t size = _this->udpHeader & 0b1111111111111;

        spdlog::warn("UDP received: {0} {1}", type, size);

        // Read the rest of the data
        if (size > 2) {
            _this->client->read(size - 2, &_this->rbuffer[2]);
        }

        // Restart an async read
        _this->client->readAsync(sizeof(_this->udpHeader), (uint8_t*)&_this->udpHeader, udpHandler, _this);
    }

    RFspaceClient connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* out) {
        net::Conn conn = net::connect(host, port);
        if (!conn) { return NULL; }
        net::Conn udpConn = net::openUDP("0.0.0.0", port, host, port, true);
        if (!udpConn) { return NULL; }
        return RFspaceClient(new RFspaceClientClass(std::move(conn), std::move(udpConn), out));
    }
}