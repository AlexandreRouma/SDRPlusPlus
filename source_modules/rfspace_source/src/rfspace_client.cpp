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

        // Allocate buffers
        rbuffer = new uint8_t[RFSPACE_MAX_SIZE];
        sbuffer = new uint8_t[RFSPACE_MAX_SIZE];
        ubuffer = new uint8_t[RFSPACE_MAX_SIZE];

        // Clear write stop of stream just in case
        output->clearWriteStop();

        // Send UDP packet so that a router opens the port
        sendDummyUDP();

        // Start readers
        client->readAsync(sizeof(tcpHeader), (uint8_t*)&tcpHeader, tcpHandler, this);
        udpClient->readAsync(RFSPACE_MAX_SIZE, ubuffer, udpHandler, this);

        // Get device ID and wait for response
        getControlItem(RFSPACE_CTRL_ITEM_PROD_ID, NULL, 0);
        {
            std::unique_lock<std::mutex> lck(devIdMtx);
            if (!devIdCnd.wait_for(lck, std::chrono::milliseconds(RFSPACE_TIMEOUT_MS), [=](){ return devIdAvailable; })) {
                throw std::runtime_error("Could not identify remote device");
            }
        }

        // Default configuration
        stop();
        setSampleRate(1228800);
        setFrequency(8830000);
        setGain(0);
        setPort(RFSPACE_RF_PORT_1);

        // Start heartbeat
        heartBeatThread = std::thread(&RFspaceClientClass::heartBeatWorker, this);
    }

    RFspaceClientClass::~RFspaceClientClass() {
        close();
        delete[] rbuffer;
        delete[] sbuffer;
        delete[] ubuffer;
    }

    void RFspaceClientClass::sendDummyUDP() {
        uint8_t dummy = 0x5A;
        udpClient->write(1, &dummy);
    }

    int RFspaceClientClass::getControlItem(ControlItem item, void* param, int len) {
        // Build packet
        uint16_t* header = (uint16_t*)&sbuffer[0];
        uint16_t* item_val = (uint16_t*)&sbuffer[2];
        *header = 4 | (RFSPACE_MSG_TYPE_H2T_REQ_CTRL_ITEM << 13);
        *item_val = item;

        // Send packet
        client->write(4, sbuffer);

        return -1;
    }

    void RFspaceClientClass::setControlItem(ControlItem item, void* param, int len) {
        // Build packet
        uint16_t* header = (uint16_t*)&sbuffer[0];
        uint16_t* item_val = (uint16_t*)&sbuffer[2];
        *header = (len + 4) | (RFSPACE_MSG_TYPE_H2T_SET_CTRL_ITEM << 13);
        *item_val = item;
        memcpy(&sbuffer[4], param, len);

        // Send packet
        client->write(len + 4, sbuffer);
    }

    void RFspaceClientClass::setControlItemWithChanID(ControlItem item, uint8_t chanId, void* param, int len) {
        // Build packet
        uint16_t* header = (uint16_t*)&sbuffer[0];
        uint16_t* item_val = (uint16_t*)&sbuffer[2];
        uint8_t* chan = &sbuffer[4];
        *header = (len + 5) | (RFSPACE_MSG_TYPE_H2T_SET_CTRL_ITEM << 13);
        *item_val = item;
        *chan = chanId;
        memcpy(&sbuffer[5], param, len);

        // Send packet
        client->write(len + 5, sbuffer);
    }

    std::vector<uint32_t> RFspaceClientClass::getValidSampleRates() {
        std::vector<uint32_t> sr;

        switch (deviceId) {
        case RFSPACE_DEV_ID_CLOUD_SDR:
        case RFSPACE_DEV_ID_CLOUD_IQ:
            for (int n = 122880000 / (4 * 25); n >= 32000; n /= 2) {
                sr.push_back(n);
            }
            break;
        case RFSPACE_DEV_ID_NET_SDR:
            for (int n = 80000000 / (4 * 25); n >= 32000; n /= 2) {
                sr.push_back(n);
            }
            break;
        default:
            break;
        }

        return sr;
    }

    void RFspaceClientClass::setFrequency(uint64_t freq) {
        setControlItemWithChanID(RFSPACE_CTRL_ITEM_NCO_FREQUENCY, 0, &freq, 5);
    }

    void RFspaceClientClass::setPort(RFPort port) {
        uint8_t value = port;
        setControlItemWithChanID(RFSPACE_CTRL_ITEM_RF_PORT, 0, &value, sizeof(value));
    }

    void RFspaceClientClass::setGain(int8_t gain) {
        setControlItemWithChanID(RFSPACE_CTRL_ITEM_RF_GAIN, 0, &gain, sizeof(gain));
    }

    void RFspaceClientClass::setSampleRate(uint32_t sampleRate) {
        setControlItemWithChanID(RFSPACE_CTRL_ITEM_IQ_SAMP_RATE, 0, &sampleRate, sizeof(sampleRate));
    }

    void RFspaceClientClass::start(SampleFormat sampleFormat, SampleDepth sampleDepth) {
        uint8_t args[4] = { (uint8_t)sampleFormat, (uint8_t)RFSPACE_STATE_RUN, (uint8_t)sampleDepth, 0 };
        setControlItem(RFSPACE_CTRL_ITEM_STATE, args, sizeof(args));
    }

    void RFspaceClientClass::stop() {
        uint8_t args[4] = { 0, RFSPACE_STATE_IDLE, 0, 0 };
        setControlItem(RFSPACE_CTRL_ITEM_STATE, args, sizeof(args));
    }

    void RFspaceClientClass::close() {
        output->stopWriter();
        stopHeartBeat = true;
        heartBeatCnd.notify_all();
        if (heartBeatThread.joinable()) { heartBeatThread.join(); }
        client->close();
        udpClient->close();
        output->clearWriteStop();
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

        // spdlog::warn("TCP received: {0} {1}", type, size);

        // Check for a device ID
        uint16_t* controlItem = (uint16_t*)&_this->rbuffer[2];
        if (type == RFSPACE_MSG_TYPE_T2H_SET_CTRL_ITEM_RESP && *controlItem == RFSPACE_CTRL_ITEM_PROD_ID) {
            {
                std::lock_guard<std::mutex> lck(_this->devIdMtx);
                _this->deviceId = (DeviceID)*(uint32_t*)&_this->rbuffer[4];
                _this->devIdAvailable = true;
            }
            _this->devIdCnd.notify_all();
        }

        // Restart an async read
        _this->client->readAsync(sizeof(_this->tcpHeader), (uint8_t*)&_this->tcpHeader, tcpHandler, _this);
    }

    void RFspaceClientClass::udpHandler(int count, uint8_t* buf, void* ctx) {
        RFspaceClientClass* _this = (RFspaceClientClass*)ctx;
        uint16_t hdr = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
        uint8_t type = hdr >> 13;
        uint16_t size = hdr & 0b1111111111111;

        if (type == RFSPACE_MSG_TYPE_T2H_DATA_ITEM_0) {
            int16_t* samples = (int16_t*)&buf[4];
            int sampCount = (size - 4) / (2 * sizeof(int16_t));
            volk_16i_s32f_convert_32f((float*)_this->output->writeBuf, samples, 32768.0f, sampCount * 2);
            _this->output->swap(sampCount);
        }

        // Restart an async read
        _this->udpClient->readAsync(RFSPACE_MAX_SIZE, _this->ubuffer, udpHandler, _this);
    }

    void RFspaceClientClass::heartBeatWorker() {
        uint8_t dummy[4];
        while (true) {
            getControlItem(RFSPACE_CTRL_ITEM_STATE, dummy, sizeof(dummy));

            std::unique_lock<std::mutex> lck(heartBeatMtx);
            bool to = heartBeatCnd.wait_for(lck, std::chrono::milliseconds(RFSPACE_HEARTBEAT_INTERVAL_MS), [=](){ return stopHeartBeat; });
            
            if (to) { return; }
        }
    }

    RFspaceClient connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* out) {
        net::Conn conn = net::connect(host, port);
        if (!conn) { return NULL; }
        net::Conn udpConn = net::openUDP("0.0.0.0", port, host, port, true);
        if (!udpConn) { return NULL; }
        return RFspaceClient(new RFspaceClientClass(std::move(conn), std::move(udpConn), out));
    }
}
