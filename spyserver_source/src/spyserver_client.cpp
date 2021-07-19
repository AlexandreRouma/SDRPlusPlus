#include <spyserver_client.h>
#include <volk/volk.h>
#include <cstring>

using namespace std::chrono_literals;

namespace spyserver {
    SpyServerClientClass::SpyServerClientClass(net::Conn conn, dsp::stream<dsp::complex_t>* out) {
        readBuf = new uint8_t[SPYSERVER_MAX_MESSAGE_BODY_SIZE];
        writeBuf = new uint8_t[SPYSERVER_MAX_MESSAGE_BODY_SIZE];
        client = std::move(conn);
        output = out;

        sendHandshake("SDR++");

        client->readAsync(sizeof(SpyServerMessageHeader), (uint8_t*)&receivedHeader, dataHandler, this);
    }

    SpyServerClientClass::~SpyServerClientClass() {
        close();
        delete[] readBuf;
        delete[] writeBuf;
    }

    void SpyServerClientClass::startStream() {
        setSetting(SPYSERVER_SETTING_STREAMING_ENABLED, true);
    }

    void SpyServerClientClass::stopStream() {
        setSetting(SPYSERVER_SETTING_STREAMING_ENABLED, false);
    }

    void SpyServerClientClass::close() {
        client->close();
    }

    bool SpyServerClientClass::isOpen() {
        return client->isOpen();
    }

    bool SpyServerClientClass::waitForDevInfo(int timeoutMS) {
        std::unique_lock lck(deviceInfoMtx);
        auto now = std::chrono::system_clock::now();
        deviceInfoCnd.wait_until(lck, now + (timeoutMS*1ms), [this](){ return deviceInfoAvailable; });
        return deviceInfoAvailable;
    }

    void SpyServerClientClass::sendCommand(uint32_t command, void* data, int len) {
        SpyServerCommandHeader* hdr = (SpyServerCommandHeader*)writeBuf;
        hdr->CommandType = command;
        hdr->BodySize = len;
        memcpy(&writeBuf[sizeof(SpyServerCommandHeader)], data, len);
        client->write(sizeof(SpyServerCommandHeader) + len, writeBuf);
    }

    void SpyServerClientClass::sendHandshake(std::string appName) {
        int totSize = sizeof(SpyServerClientHandshake) + appName.size();
        uint8_t* buf = new uint8_t[totSize];

        SpyServerClientHandshake* cmdHandshake = (SpyServerClientHandshake*)buf;
        cmdHandshake->ProtocolVersion = SPYSERVER_PROTOCOL_VERSION;

        memcpy(&buf[sizeof(SpyServerClientHandshake)], appName.c_str(), appName.size());
        sendCommand(SPYSERVER_CMD_HELLO, buf, totSize);

        delete[] buf;
    }

    void SpyServerClientClass::setSetting(uint32_t setting, uint32_t arg) {
        SpyServerSettingTarget target;
        target.Setting = setting;
        target.Value = arg;
        sendCommand(SPYSERVER_CMD_SET_SETTING, &target, sizeof(SpyServerSettingTarget));
    }

    int SpyServerClientClass::readSize(int count, uint8_t* buffer) {
        int read = 0;
        int len = 0;
        while (read < count) {
            len = client->read(count - read, &buffer[read]);
            if (len <= 0) { return len; }
            read += len;
        }
        return read;
    }

    void SpyServerClientClass::dataHandler(int count, uint8_t* buf, void* ctx) {
        SpyServerClientClass* _this = (SpyServerClientClass*)ctx;

        int size = _this->readSize(_this->receivedHeader.BodySize, _this->readBuf);
        if (size <= 0) {
            printf("ERROR: Didn't receive enough bytes\n");
            return;
        }

        //printf("MSG %08X %d %d %08X %d\n", _this->receivedHeader.ProtocolID, _this->receivedHeader.MessageType, _this->receivedHeader.StreamType, _this->receivedHeader.SequenceNumber, _this->receivedHeader.BodySize);

        if (_this->receivedHeader.MessageType == SPYSERVER_MSG_TYPE_DEVICE_INFO) {
            {
                std::lock_guard lck(_this->deviceInfoMtx);
                SpyServerDeviceInfo* _devInfo = (SpyServerDeviceInfo*)_this->readBuf;
                _this->devInfo = *_devInfo;
                _this->deviceInfoAvailable = true;
            }
            _this->deviceInfoCnd.notify_all();
        }
        else if (_this->receivedHeader.MessageType == SPYSERVER_MSG_TYPE_UINT8_IQ) {
            int sampCount = _this->receivedHeader.BodySize / (sizeof(uint8_t)*2);
            for (int i = 0; i < sampCount; i++) {
                _this->output->writeBuf[i].re = ((float)_this->readBuf[(2*i)] / 128.0f)-1.0f;
                _this->output->writeBuf[i].im = ((float)_this->readBuf[(2*i)+1] / 128.0f)-1.0f;
            }
            _this->output->swap(sampCount);
        }
        else if (_this->receivedHeader.MessageType == SPYSERVER_MSG_TYPE_INT16_IQ) {
            int sampCount = _this->receivedHeader.BodySize / (sizeof(int16_t)*2);
            volk_16i_s32f_convert_32f((float*)_this->output->writeBuf, (int16_t*)_this->readBuf, 32768.0, sampCount*2);
            _this->output->swap(sampCount);
        }
        else if (_this->receivedHeader.MessageType == SPYSERVER_MSG_TYPE_INT24_IQ) {
            printf("ERROR: IQ format not supported\n");
            return;
        }
        else if (_this->receivedHeader.MessageType == SPYSERVER_MSG_TYPE_FLOAT_IQ) {
            int sampCount = _this->receivedHeader.BodySize / sizeof(dsp::complex_t);
            memcpy(_this->output->writeBuf, _this->readBuf, _this->receivedHeader.BodySize);
            _this->output->swap(sampCount);
        }

        _this->client->readAsync(sizeof(SpyServerMessageHeader), (uint8_t*)&_this->receivedHeader, dataHandler, _this);
    }

    SpyServerClient connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* out) {
        net::Conn conn = net::connect(net::PROTO_TCP, host, port);
        if (!conn) {
            return NULL;
        }
        return SpyServerClient(new SpyServerClientClass(std::move(conn), out));
    }
}