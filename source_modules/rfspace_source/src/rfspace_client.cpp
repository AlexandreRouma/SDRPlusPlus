#include <rfspace_client.h>
#include <volk/volk.h>
#include <cstring>
#include <utils/flog.h>

using namespace std::chrono_literals;

namespace rfspace {
    Client::Client(std::shared_ptr<net::Socket> tcp, std::shared_ptr<net::Socket> udp, dsp::stream<dsp::complex_t>* out) {
        this->tcp = tcp;
        this->udp = udp;
        output = out;

        // Allocate buffers
        sbuffer = new uint8_t[RFSPACE_MAX_SIZE];

        // Clear write stop of stream just in case
        output->clearWriteStop();

        // Send UDP packet so that a router opens the port
        sendDummyUDP();

        // Start workers
        tcpWorkerThread = std::thread(&Client::tcpWorker, this);
        udpWorkerThread = std::thread(&Client::udpWorker, this);

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
        heartBeatThread = std::thread(&Client::heartBeatWorker, this);
    }

    Client::~Client() {
        close();
        delete[] sbuffer;
    }

    void Client::sendDummyUDP() {
        uint8_t dummy = 0x5A;
        udp->send(&dummy, 1);
    }

    int Client::getControlItem(ControlItem item, void* param, int len) {
        // Build packet
        uint16_t* header = (uint16_t*)&sbuffer[0];
        uint16_t* item_val = (uint16_t*)&sbuffer[2];
        *header = 4 | (RFSPACE_MSG_TYPE_H2T_REQ_CTRL_ITEM << 13);
        *item_val = item;

        // Send packet
        tcp->send(sbuffer, 4);

        return -1;
    }

    void Client::setControlItem(ControlItem item, void* param, int len) {
        // Build packet
        uint16_t* header = (uint16_t*)&sbuffer[0];
        uint16_t* item_val = (uint16_t*)&sbuffer[2];
        *header = (len + 4) | (RFSPACE_MSG_TYPE_H2T_SET_CTRL_ITEM << 13);
        *item_val = item;
        memcpy(&sbuffer[4], param, len);

        // Send packet
        tcp->send(sbuffer, len + 4);
    }

    void Client::setControlItemWithChanID(ControlItem item, uint8_t chanId, void* param, int len) {
        // Build packet
        uint16_t* header = (uint16_t*)&sbuffer[0];
        uint16_t* item_val = (uint16_t*)&sbuffer[2];
        uint8_t* chan = &sbuffer[4];
        *header = (len + 5) | (RFSPACE_MSG_TYPE_H2T_SET_CTRL_ITEM << 13);
        *item_val = item;
        *chan = chanId;
        memcpy(&sbuffer[5], param, len);

        // Send packet
        tcp->send(sbuffer, len + 5);
    }

    std::vector<uint32_t> Client::getSamplerates() {
        std::vector<uint32_t> sr;

        switch (deviceId) {
        case RFSPACE_DEV_ID_CLOUD_SDR:
        case RFSPACE_DEV_ID_CLOUD_IQ:
            for (int n = 122880000 / (4 * 25); n >= 32000; n /= 2) {
                sr.push_back(n);
            }
            break;
        case RFSPACE_DEV_ID_NET_SDR:
        case RFSPACE_DEV_ID_SDR_IP:
        default:
            for (int n = 80000000 / (4 * 25); n >= 32000; n /= 2) {
                sr.push_back(n);
            }
            break;
        }

        return sr;
    }

    void Client::setFrequency(uint64_t freq) {
        setControlItemWithChanID(RFSPACE_CTRL_ITEM_NCO_FREQUENCY, 0, &freq, 5);
    }

    void Client::setPort(RFPort port) {
        uint8_t value = port;
        setControlItemWithChanID(RFSPACE_CTRL_ITEM_RF_PORT, 0, &value, sizeof(value));
    }

    void Client::setGain(int8_t gain) {
        setControlItemWithChanID(RFSPACE_CTRL_ITEM_RF_GAIN, 0, &gain, sizeof(gain));
    }

    void Client::setSampleRate(uint32_t sampleRate) {
        // Acquire the buffer variables
        std::lock_guard<std::mutex> lck(bufferMtx);

        // Update block size
        blockSize = sampleRate / 200;

        // Send samplerate to device
        setControlItemWithChanID(RFSPACE_CTRL_ITEM_IQ_SAMP_RATE, 0, &sampleRate, sizeof(sampleRate));
    }

    void Client::start(SampleFormat sampleFormat, SampleDepth sampleDepth) {
        // Acquire the buffer variables
        std::lock_guard<std::mutex> lck(bufferMtx);
        
        // Reset buffer
        inBuffer = 0;

        // Start device
        uint8_t args[4] = { (uint8_t)sampleFormat, (uint8_t)RFSPACE_STATE_RUN, (uint8_t)sampleDepth, 0 };
        setControlItem(RFSPACE_CTRL_ITEM_STATE, args, sizeof(args));
    }

    void Client::stop() {
        uint8_t args[4] = { 0, RFSPACE_STATE_IDLE, 0, 0 };
        setControlItem(RFSPACE_CTRL_ITEM_STATE, args, sizeof(args));
    }

    void Client::close() {
        // Stop UDP worker
        output->stopWriter();
        udp->close();
        if (udpWorkerThread.joinable()) { udpWorkerThread.join(); }
        output->clearWriteStop();

        // Stop heartbeat worker
        stopHeartBeat = true;
        heartBeatCnd.notify_all();
        if (heartBeatThread.joinable()) { heartBeatThread.join(); }

        // Stop TCP worker
        tcp->close();
        if (tcpWorkerThread.joinable()) { tcpWorkerThread.join(); }
    }

    bool Client::isOpen() {
        return tcp->isOpen() || udp->isOpen();
    }

    void Client::tcpWorker() {
        // Allocate receive buffer
        uint8_t* buffer = new uint8_t[RFSPACE_MAX_SIZE];

        // Receive loop
        while (true) {
            // Receive header
            uint16_t header;
            if (tcp->recv((uint8_t*)&header, sizeof(uint16_t), true) <= 0) { break; }

            // Decode header
            uint8_t type = header >> 13;
            uint16_t size = header & 0b1111111111111;

            // Receive data
            if (tcp->recv(buffer, size - 2, true, RFSPACE_TIMEOUT_MS) <= 0) { break; }
            
            // Check for a device ID
            uint16_t* controlItem = (uint16_t*)&buffer[0];
            if (type == RFSPACE_MSG_TYPE_T2H_SET_CTRL_ITEM_RESP && *controlItem == RFSPACE_CTRL_ITEM_PROD_ID) {
                {
                    std::lock_guard<std::mutex> lck(devIdMtx);
                    deviceId = (DeviceID)*(uint32_t*)&buffer[2];
                    devIdAvailable = true;
                }
                devIdCnd.notify_all();
            }
        }

        // Free receive buffer
        delete[] buffer;
    }

    void Client::udpWorker() {
        // Allocate receive buffer
        uint8_t* buffer = new uint8_t[RFSPACE_MAX_SIZE];
        uint16_t* header = (uint16_t*)&buffer[0];

        // Receive loop
        while (true) {
            // Receive datagram
            int rsize = udp->recv(buffer, RFSPACE_MAX_SIZE);
            if (rsize <= 0) { break; } 

            // Decode header
            uint8_t type = (*header) >> 13;
            uint16_t size = (*header) & 0b1111111111111;

            if (rsize != size) {
                flog::error("Datagram size mismatch: {} vs {}", rsize, size);
                continue;
            }

            // Check for a sample packet
            if (type == RFSPACE_MSG_TYPE_T2H_DATA_ITEM_0) {
                // Acquire the buffer variables
                std::lock_guard<std::mutex> lck(bufferMtx);

                // Convert samples to complex float
                int16_t* samples = (int16_t*)&buffer[4];
                int sampCount = (size - 4) / (2 * sizeof(int16_t));
                volk_16i_s32f_convert_32f((float*)&output->writeBuf[inBuffer], samples, 32768.0f, sampCount * 2);
                inBuffer += sampCount;

                // Send out samples if enough are buffered
                if (inBuffer >= blockSize) {
                    if (!output->swap(inBuffer)) { break; };
                    inBuffer = 0;
                }
            }
        }

        // Free receive buffer
        delete[] buffer;
    }

    void Client::heartBeatWorker() {
        uint8_t dummy[4];
        while (true) {
            getControlItem(RFSPACE_CTRL_ITEM_STATE, dummy, sizeof(dummy));

            std::unique_lock<std::mutex> lck(heartBeatMtx);
            bool to = heartBeatCnd.wait_for(lck, std::chrono::milliseconds(RFSPACE_HEARTBEAT_INTERVAL_MS), [=](){ return stopHeartBeat; });
            
            if (to) { return; }
        }
    }

    std::shared_ptr<Client> connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* out) {
        auto tcp = net::connect(host, port);
        auto udp = net::openudp(host, port, "0.0.0.0", port);
        return std::make_shared<Client>(tcp, udp, out);
    }
}
