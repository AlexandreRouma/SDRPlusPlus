#include "hermes.h"
#include <spdlog/spdlog.h>

namespace hermes {
    Client::Client(std::shared_ptr<net::Socket> sock) {
        this->sock = sock;

        // Start worker
        workerThread = std::thread(&Client::worker, this);
    }

    void Client::close() {
        if (!open) { return; }
        sock->close();

        // Wait for worker to exit
        if (workerThread.joinable()) { workerThread.join(); }

        open = false;
    }

    void Client::start() {
        sendMetisControl((MetisControl)(METIS_CTRL_IQ | METIS_CTRL_NO_WD));
    }

    void Client::stop() {
        sendMetisControl(METIS_CTRL_NONE);
    }

    void Client::setSamplerate(HermesLiteSamplerate samplerate) {
        writeReg(0, (uint32_t)samplerate << 24);
    }

    void Client::setFrequency(double freq) {
        writeReg(HL_REG_TX1_NCO_FREQ, freq);
    }

    void Client::setGain(int gain) {
        writeReg(HL_REG_RX_LNA, gain | (1 << 6));
    }

    void Client::sendMetisUSB(uint8_t endpoint, void* frame0, void* frame1) {
        // Build packet
        uint32_t seq = usbSeq++;
        MetisUSBPacket pkt;
        pkt.hdr.signature = htons(HERMES_METIS_SIGNATURE);
        pkt.hdr.type = METIS_PKT_USB;
        pkt.endpoint = endpoint;
        pkt.seq = htonl(seq);
        if (frame0) { memcpy(pkt.frame[0], frame0, 512); }
        else { memset(pkt.frame[0], 0, 512); }
        if (frame1) { memcpy(pkt.frame[1], frame1, 512); }
        else { memset(pkt.frame[1], 0, 512); }

        // Send packet
        sock->send((uint8_t*)&pkt, sizeof(pkt));
    }

    void Client::sendMetisControl(MetisControl ctrl) {
        // Build packet
        MetisControlPacket pkt;
        pkt.hdr.signature = htons(HERMES_METIS_SIGNATURE);
        pkt.hdr.type = METIS_PKT_CONTROL;
        pkt.ctrl = ctrl;

        // Send packet
        sock->send((uint8_t*)&pkt, sizeof(pkt));
    }

    uint32_t Client::readReg(uint8_t addr) {
        uint8_t frame[512];
        memset(frame, 0, sizeof(frame));

        HPSDRUSBHeader* hdr = (HPSDRUSBHeader*)frame;
        hdr->sync[0] = HERMES_HPSDR_USB_SYNC;
        hdr->sync[1] = HERMES_HPSDR_USB_SYNC;
        hdr->sync[2] = HERMES_HPSDR_USB_SYNC;
        hdr->c0 = (addr << 1) | (1 << 7);

        sendMetisUSB(2, frame);

        return 0;
    }

    void Client::writeReg(uint8_t addr, uint32_t val) {
        uint8_t frame[512];
        memset(frame, 0, sizeof(frame));

        HPSDRUSBHeader* hdr = (HPSDRUSBHeader*)frame;
        hdr->sync[0] = HERMES_HPSDR_USB_SYNC;
        hdr->sync[1] = HERMES_HPSDR_USB_SYNC;
        hdr->sync[2] = HERMES_HPSDR_USB_SYNC;
        hdr->c0 = addr << 1;
        *(uint32_t*)hdr->c = htonl(val);

        sendMetisUSB(2, frame);
    }

    void Client::worker() {
        uint8_t rbuf[2048];
        MetisUSBPacket* pkt = (MetisUSBPacket*)rbuf;
        while (true) {
            // Wait for a packet or exit if connection closed
            int len = sock->recv(rbuf, 2048);
            if (len <= 0) { break; }

            // Ignore anything that's not a USB packet
            if (htons(pkt->hdr.signature) != HERMES_METIS_SIGNATURE || pkt->hdr.type != METIS_PKT_USB) {
                continue;
            }

            // Parse frames
            for (int frn = 0; frn < 2; frn++) {
                uint8_t* frame = pkt->frame[frn];
                HPSDRUSBHeader* hdr = (HPSDRUSBHeader*)frame;

                // Make sure this is a valid frame by checking the sync
                if (hdr->sync[0] != 0x7F || hdr->sync[1] != 0x7F || hdr->sync[2] != 0x7F) {
                    continue;
                }

                // Check if this is a response
                if (hdr->c0 & (1 << 7)) {
                    uint8_t reg = (hdr->c0 >> 1) & 0x3F;
                    spdlog::warn("Got response! Reg={0}, Seq={1}", reg, htonl(pkt->seq));
                }

                // Decode and send IQ to stream
                uint8_t* iq = &frame[8];
                for (int i = 0; i < 63; i++) {
                    // Convert to 32bit
                    int32_t si = ((uint32_t)iq[(i*8) + 0] << 16) | ((uint32_t)iq[(i*8) + 1] << 8) | (uint32_t)iq[(i*8) + 2];
                    int32_t sq = ((uint32_t)iq[(i*8) + 3] << 16) | ((uint32_t)iq[(i*8) + 4] << 8) | (uint32_t)iq[(i*8) + 5];
                    
                    // Sign extend
                    si = (si << 8) >> 8;
                    sq = (sq << 8) >> 8;

                    // Convert to float (IQ swapper for some reason... 'I' means in-phase... :facepalm:)
                    out.writeBuf[i].im = (float)si / (float)0x1000000;
                    out.writeBuf[i].re = (float)sq / (float)0x1000000;
                }
                out.swap(63);
                // TODO: Buffer the data to avoid having a very high DSP frame rate
            }            
        }
    }

    std::vector<Info> discover() {
        auto sock = net::openudp("192.168.0.255", 1024);
        
        // Build discovery packet
        uint8_t discoveryPkt[64];
        memset(discoveryPkt, 0, sizeof(discoveryPkt));
        *(uint16_t*)&discoveryPkt[0] = htons(HERMES_METIS_SIGNATURE);
        discoveryPkt[2] = METIS_PKT_DISCOVER;

        // Send the packet 5 times to make sure it's received
        for (int i = 0; i < HERMES_DISCOVER_REPEAT; i++) {
            sock->send(discoveryPkt, sizeof(discoveryPkt));
        }

        std::vector<Info> devices;

        while (true) {
            // Wait for a response
            net::Address addr;
            uint8_t resp[1024];
            int len = sock->recv(resp, sizeof(resp), false, HERMES_DISCOVER_TIMEOUT, &addr);
            
            // Give up if timeout or error
            if (len <= 0) { break; }

            // Verify that it is a valid response
            if (len < 60) { continue; }
            if (resp[0] != 0xEF || resp[1] != 0xFE) { continue; }

            // Analyze
            Info info;
            info.addr = addr;
            memcpy(info.mac, &resp[3], 6);
            info.gatewareVerMaj = resp[0x09];
            info.gatewareVerMin = resp[0x15];

            // Check if the device is already in the list
            bool found = false;
            for (const auto& d : devices) {
                if (!memcmp(info.mac, d.mac, 6)) {
                    found = true;
                    break;
                }
            }
            if (found) { continue; }

            devices.push_back(info);
        }
        

        return devices;
    }

    std::shared_ptr<Client> open(std::string host, int port) {
        return open(net::Address(host, port));
    }

    std::shared_ptr<Client> open(const net::Address& addr) {
        // Open UDP socket
        auto sock = net::openudp(addr);

        // TODO: Check if open successful
        return std::make_shared<Client>(sock);
    }
}