#include "hermes.h"
#include <utils/flog.h>

namespace hermes {
    const int SAMPLERATE_LIST[] = {
        48000,
        96000,
        192000,
        384000
    };

    Client::Client(std::shared_ptr<net::Socket> sock) {
        this->sock = sock;

        // Start worker
        workerThread = std::thread(&Client::worker, this);
    }

    void Client::close() {
        sock->close();

        // Wait for worker to exit
        out.stopWriter();
        if (workerThread.joinable()) { workerThread.join(); }
        out.clearWriteStop();
    }

    void Client::start() {
        // Start metis stream
        for (int i = 0; i < HERMES_METIS_REPEAT; i++) {
            sendMetisControl((MetisControl)(METIS_CTRL_IQ | METIS_CTRL_NO_WD));
        }
    }

    void Client::stop() {
        for (int i = 0; i < HERMES_METIS_REPEAT; i++) {
            sendMetisControl(METIS_CTRL_NONE);
        }
    }

    void Client::setSamplerate(HermesLiteSamplerate samplerate) {
        writeReg(0, (uint32_t)samplerate << 24);
        blockSize = SAMPLERATE_LIST[samplerate] / 200;
    }

    void Client::setFrequency(double freq) {
        this->freq = freq;
        writeReg(HL_REG_TX1_NCO_FREQ, freq);
        autoFilters(freq);
    }

    void Client::setGain(int gain) {
        writeReg(HL_REG_RX_LNA, gain | (1 << 6));
    }

    void Client::autoFilters(double freq) {
        uint8_t filt = (freq >= 3000000.0) ? (1 << 6) : 0;
        
        if (freq <= 2000000.0) {
            filt |= (1 << 0);
        }
        else if (freq <= 4000000.0) {
            filt |= (1 << 1);
        }
        else if (freq <= 7300000.0) {
            filt |= (1 << 2);
        }
        else if (freq <= 14350000.0) {
            filt |= (1 << 3);
        }
        else if (freq <= 21450000.0) {
            filt |= (1 << 4);
        }
        else if (freq <= 29700000.0) {
            filt |= (1 << 5);
        }

        // Write only if the config actually changed
        if (filt != lastFilt) {
            lastFilt = filt;

            flog::warn("Setting filters");

            // Set direction and wait for things to be processed
            writeI2C(I2C_PORT_2, 0x20, 0x00, 0x00);

            // Set pins
            writeI2C(I2C_PORT_2, 0x20, 0x0A, filt);
        }
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

        // TODO: Wait for response

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

    void Client::writeI2C(I2CPort port, uint8_t addr, uint8_t reg, uint8_t data) {
        uint32_t wdata = data;
        wdata |= reg << 8;
        wdata |= (addr & 0x7F) << 16;
        wdata |= 1 << 23;
        wdata |= 0x06 << 24;
        writeReg(HL_REG_I2C_1 + port, wdata);
#ifdef _WIN32
            Sleep(HERMES_I2C_DELAY);
#else
            usleep(HERMES_I2C_DELAY*1000);
#endif
    }

    void Client::worker() {
        uint8_t rbuf[2048];
        MetisUSBPacket* pkt = (MetisUSBPacket*)rbuf;
        int sampleCount = 0;

        while (true) {
            // Wait for a packet or exit if connection closed
            int len = sock->recv(rbuf, 2048);
            if (len <= 0) { break; }

            // Ignore anything that's not a USB packet
            // TODO: Gotta check the endpoint
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
                    flog::warn("Got response! Reg={0}, Seq={1}", reg, (uint32_t)htonl(pkt->seq));
                }

                // Decode and save IQ to buffer
                uint8_t* iq = &frame[8];
                dsp::complex_t* writeBuf = &out.writeBuf[sampleCount];
                for (int i = 0; i < HERMES_SAMPLES_PER_FRAME; i++) {
                    // Convert to 32bit
                    int32_t si = ((uint32_t)iq[(i*8) + 0] << 16) | ((uint32_t)iq[(i*8) + 1] << 8) | (uint32_t)iq[(i*8) + 2];
                    int32_t sq = ((uint32_t)iq[(i*8) + 3] << 16) | ((uint32_t)iq[(i*8) + 4] << 8) | (uint32_t)iq[(i*8) + 5];
                    
                    // Sign extend
                    si = (si << 8) >> 8;
                    sq = (sq << 8) >> 8;

                    // Convert to float (IQ swapped for some reason)
                    writeBuf[i].im = (float)si / (float)0x1000000;
                    writeBuf[i].re = (float)sq / (float)0x1000000;
                }
                sampleCount += HERMES_SAMPLES_PER_FRAME;

                // If enough samples are in the buffer, send to stream
                if (sampleCount >= blockSize) {
                    out.swap(sampleCount);
                    sampleCount = 0;
                }
            }            
        }
    }

    std::vector<Info> discover() {
        // Open a UDP broadcast socket (TODO: Figure out why 255.255.255.255 doesn't work on windows with local = 0.0.0.0)
        auto sock = net::openudp("255.255.255.255", 1024, "0.0.0.0", 0, true);
        
        // Build discovery packet
        uint8_t discoveryPkt[64];
        memset(discoveryPkt, 0, sizeof(discoveryPkt));
        *(uint16_t*)&discoveryPkt[0] = htons(HERMES_METIS_SIGNATURE);
        discoveryPkt[2] = METIS_PKT_DISCOVER;

        // Get interface list
        auto ifaces = net::listInterfaces();

        // Send the packet 5 times to make sure it's received
        for (const auto& [name, iface] : ifaces) {
            net::Address baddr(iface.broadcast, 1024);
            for (int i = 0; i < HERMES_METIS_REPEAT; i++) {
                sock->send(discoveryPkt, sizeof(discoveryPkt), &baddr);
            }
        }

        // Await all responses
        std::vector<Info> devices;
        while (true) {
            // Wait for a response
            net::Address addr;
            uint8_t resp[1024];
            int len = sock->recv(resp, sizeof(resp), false, HERMES_METIS_TIMEOUT, &addr);
            
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

        // Close broadcast socket
        sock->close();        

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