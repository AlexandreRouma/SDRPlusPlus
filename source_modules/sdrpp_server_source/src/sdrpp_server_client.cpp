#include "sdrpp_server_client.h"
#include <volk/volk.h>
#include <cstring>
#include <spdlog/spdlog.h>
#include <core.h>

using namespace std::chrono_literals;

namespace server {
    ClientClass::ClientClass(net::Conn conn, dsp::stream<dsp::complex_t>* out) {
        client = std::move(conn);
        output = out;

        // Allocate buffers
        rbuffer = new uint8_t[SERVER_MAX_PACKET_SIZE];
        sbuffer = new uint8_t[SERVER_MAX_PACKET_SIZE];

        // Initialize headers
        r_pkt_hdr = (PacketHeader*)rbuffer;
        r_pkt_data = &rbuffer[sizeof(PacketHeader)];
        r_cmd_hdr = (CommandHeader*)r_pkt_data;
        r_cmd_data = &rbuffer[sizeof(PacketHeader) + sizeof(CommandHeader)];

        s_pkt_hdr = (PacketHeader*)sbuffer;
        s_pkt_data = &sbuffer[sizeof(PacketHeader)];
        s_cmd_hdr = (CommandHeader*)s_pkt_data;
        s_cmd_data = &sbuffer[sizeof(PacketHeader) + sizeof(CommandHeader)];

        // Initialize decompressor
        dctx = ZSTD_createDCtx();

        // Initialize DSP
        decompIn.setBufferSize((sizeof(dsp::complex_t) * STREAM_BUFFER_SIZE) + 8);
        decompIn.clearWriteStop();
        decomp.init(&decompIn);
        link.init(&decomp.out, output);
        decomp.start();
        link.start();

        // Start readers
        client->readAsync(sizeof(PacketHeader), rbuffer, tcpHandler, this);

        // Ask for a UI
        int res = getUI();
        if (res == -1) { throw std::runtime_error("Timed out"); }
        else if (res == -2) { throw std::runtime_error("Server busy"); }
    }

    ClientClass::~ClientClass() {
        close();
        ZSTD_freeDCtx(dctx);
        delete[] rbuffer;
        delete[] sbuffer;
    }

    void ClientClass::showMenu() {
        std::string diffId = "";
        SmGui::DrawListElem diffValue;
        bool syncRequired = false;
        {
            std::lock_guard<std::mutex> lck(dlMtx);
            dl.draw(diffId, diffValue, syncRequired);
        }

        if (!diffId.empty()) {
            // Save ID
            SmGui::DrawListElem elemId;
            elemId.type = SmGui::DRAW_LIST_ELEM_TYPE_STRING;
            elemId.str = diffId;

            // Encore packet
            int size = 0;
            s_cmd_data[size++] = syncRequired;
            size += SmGui::DrawList::storeItem(elemId, &s_cmd_data[size], SERVER_MAX_PACKET_SIZE - size);
            size += SmGui::DrawList::storeItem(diffValue, &s_cmd_data[size], SERVER_MAX_PACKET_SIZE - size);

            // Send
            if (syncRequired) {
                spdlog::warn("Action requires resync");
                auto waiter = awaitCommandAck(COMMAND_UI_ACTION);
                sendCommand(COMMAND_UI_ACTION, size);
                if (waiter->await(PROTOCOL_TIMEOUT_MS)) {
                    std::lock_guard lck(dlMtx);
                    dl.load(r_cmd_data, r_pkt_hdr->size - sizeof(PacketHeader) - sizeof(CommandHeader));
                }
                else {
                    spdlog::error("Timeout out after asking for UI");
                }
                waiter->handled();
                spdlog::warn("Resync done");
            }
            else {
                spdlog::warn("Action does not require resync");
                sendCommand(COMMAND_UI_ACTION, size);
            }
        }
    }

    void ClientClass::setFrequency(double freq) {
        if (!client || !client->isOpen()) { return; }
        *(double*)s_cmd_data = freq;
        sendCommand(COMMAND_SET_FREQUENCY, sizeof(double));
        auto waiter = awaitCommandAck(COMMAND_SET_FREQUENCY);
        waiter->await(PROTOCOL_TIMEOUT_MS);
        waiter->handled();
    }

    double ClientClass::getSampleRate() {
        return currentSampleRate;
    }

    void ClientClass::setSampleType(dsp::PCMType type) {
        s_cmd_data[0] = type;
        sendCommand(COMMAND_SET_SAMPLE_TYPE, 1);
    }

    void ClientClass::setCompression(bool enabled) {
         s_cmd_data[0] = enabled;
        sendCommand(COMMAND_SET_COMPRESSION, 1);
    }

    void ClientClass::start() {
        if (!client || !client->isOpen()) { return; }
        sendCommand(COMMAND_START, 0);
        getUI();
    }

    void ClientClass::stop() {
        if (!client || !client->isOpen()) { return; }
        sendCommand(COMMAND_STOP, 0);
        getUI();
    }

    void ClientClass::close() {
        decomp.stop();
        link.stop();
        decompIn.stopWriter();
        client->close();
        decompIn.clearWriteStop();
    }

    bool ClientClass::isOpen() {
        return client->isOpen();
    }

    void ClientClass::tcpHandler(int count, uint8_t* buf, void* ctx) {
        ClientClass* _this = (ClientClass*)ctx;
        
        // Read the rest of the data (TODO: CHECK SIZE OR SHIT WILL BE FUCKED)
        int len = 0;
        int read = 0;
        int goal = _this->r_pkt_hdr->size - sizeof(PacketHeader);
        while (len < goal) {
            read = _this->client->read(goal - len, &buf[sizeof(PacketHeader) + len]);
            if (read < 0) {
                return; 
            };
            len += read;
        }
        _this->bytes += _this->r_pkt_hdr->size;
        
        if (_this->r_pkt_hdr->type == PACKET_TYPE_COMMAND) {
            // TODO: Move to command handler
            if (_this->r_cmd_hdr->cmd == COMMAND_SET_SAMPLERATE && _this->r_pkt_hdr->size == sizeof(PacketHeader) + sizeof(CommandHeader) + sizeof(double)) {
                _this->currentSampleRate = *(double*)_this->r_cmd_data;
                core::setInputSampleRate(_this->currentSampleRate);
            }
            else if (_this->r_cmd_hdr->cmd == COMMAND_DISCONNECT) {
                spdlog::error("Asked to disconnect by the server");
                _this->serverBusy = true;

                // Cancel waiters
                std::vector<PacketWaiter*> toBeRemoved;
                for (auto& [waiter, cmd] : _this->commandAckWaiters) {
                    waiter->cancel();
                    toBeRemoved.push_back(waiter);
                }

                // Remove handled waiters
                for (auto& waiter : toBeRemoved) {
                    _this->commandAckWaiters.erase(waiter);
                    delete waiter;
                }
            }
        }
        else if (_this->r_pkt_hdr->type == PACKET_TYPE_COMMAND_ACK) {
            // Notify waiters
            std::vector<PacketWaiter*> toBeRemoved;
            for (auto& [waiter, cmd] : _this->commandAckWaiters) {
                if (cmd != _this->r_cmd_hdr->cmd) { continue; }
                waiter->notify();
                toBeRemoved.push_back(waiter);
            }

            // Remove handled waiters
            for (auto& waiter : toBeRemoved) {
                _this->commandAckWaiters.erase(waiter);
                delete waiter;
            }
        }
        else if (_this->r_pkt_hdr->type == PACKET_TYPE_BASEBAND) {
            memcpy(_this->decompIn.writeBuf, &buf[sizeof(PacketHeader)], _this->r_pkt_hdr->size - sizeof(PacketHeader));
            _this->decompIn.swap(_this->r_pkt_hdr->size - sizeof(PacketHeader));
        }
        else if (_this->r_pkt_hdr->type == PACKET_TYPE_BASEBAND_COMPRESSED) {
            size_t outCount = ZSTD_decompressDCtx(_this->dctx, _this->decompIn.writeBuf, STREAM_BUFFER_SIZE, _this->r_pkt_data, _this->r_pkt_hdr->size - sizeof(PacketHeader));
            if (outCount) { _this->decompIn.swap(outCount); };
        }
        else if (_this->r_pkt_hdr->type == PACKET_TYPE_ERROR) {
            spdlog::error("SDR++ Server Error: {0}", buf[sizeof(PacketHeader)]);
        }
        else {
            spdlog::error("Invalid packet type: {0}", _this->r_pkt_hdr->type);
        }

        // Restart an async read
        _this->client->readAsync(sizeof(PacketHeader), _this->rbuffer, tcpHandler, _this);
    }

    int ClientClass::getUI() {
        auto waiter = awaitCommandAck(COMMAND_GET_UI);
        sendCommand(COMMAND_GET_UI, 0);
        if (waiter->await(PROTOCOL_TIMEOUT_MS)) {
            std::lock_guard lck(dlMtx);
            dl.load(r_cmd_data, r_pkt_hdr->size - sizeof(PacketHeader) - sizeof(CommandHeader));
        }
        else {
            if (!serverBusy) { spdlog::error("Timeout out after asking for UI"); };
            waiter->handled();
            return serverBusy ? -2 : -1;
        }
        waiter->handled();
        return 0;
    }

    void ClientClass::sendPacket(PacketType type, int len) {
        s_pkt_hdr->type = type;
        s_pkt_hdr->size = sizeof(PacketHeader) + len;
        client->write(s_pkt_hdr->size, sbuffer);
    }

    void ClientClass::sendCommand(Command cmd, int len) {
        s_cmd_hdr->cmd = cmd;
        sendPacket(PACKET_TYPE_COMMAND, sizeof(CommandHeader) + len);
    }

    void ClientClass::sendCommandAck(Command cmd, int len) {
        s_cmd_hdr->cmd = cmd;
        sendPacket(PACKET_TYPE_COMMAND_ACK, sizeof(CommandHeader) + len);
    }

    PacketWaiter* ClientClass::awaitCommandAck(Command cmd) {
        PacketWaiter* waiter = new PacketWaiter;
        commandAckWaiters[waiter] = cmd;
        return waiter;
    }

    void ClientClass::dHandler(dsp::complex_t *data, int count, void *ctx) {
        ClientClass* _this = (ClientClass*)ctx;
        memcpy(_this->output->writeBuf, data, count * sizeof(dsp::complex_t));
        _this->output->swap(count);
    }

    Client connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* out) {
        net::Conn conn = net::connect(host, port);
        if (!conn) { return NULL; }
        return Client(new ClientClass(std::move(conn), out));
    }
}
