#pragma once
#include "net.h"
#include <dsp/stream.h>
#include <dsp/types.h>
#include <memory>
#include <vector>
#include <string>
#include <thread>

#define HERMES_DISCOVER_REPEAT  5
#define HERMES_DISCOVER_TIMEOUT 1000
#define HERMES_METIS_SIGNATURE  0xEFFE
#define HERMES_HPSDR_USB_SYNC   0x7F

namespace hermes {
    enum MetisPacketType {
        METIS_PKT_USB       = 0x01,
        METIS_PKT_DISCOVER  = 0x02,
        METIS_PKT_CONTROL   = 0x04
    };

    enum MetisControl {
        METIS_CTRL_NONE     = 0,
        METIS_CTRL_IQ       = (1 << 0),
        METIS_CTRL_WIDEBAND = (1 << 1),
        METIS_CTRL_NO_WD    = (1 << 7)
    };

    enum BoardID {
        BOARD_ID_HERMES_EMUL    = 1,
        BOARD_ID_HL2            = 6
    };

    struct Info {
        uint8_t mac[6];
        uint8_t gatewareVerMaj;
        uint8_t gatewareVerMin;
        BoardID boardId;
    };

    enum HermesLiteReg {
        HL_REG_TX1_NCO_FREQ     = 0x01,
        HL_REG_RX1_NCO_FREQ     = 0x02,
        HL_REG_RX2_NCO_FREQ     = 0x03,
        HL_REG_RX3_NCO_FREQ     = 0x04,
        HL_REG_RX4_NCO_FREQ     = 0x05,
        HL_REG_RX5_NCO_FREQ     = 0x06,
        HL_REG_RX6_NCO_FREQ     = 0x07,
        HL_REG_RX7_NCO_FREQ     = 0x08,

        HL_REG_RX_LNA           = 0x0A,
        HL_REG_TX_LNA           = 0x0E,
        HL_REG_CWX              = 0x0F,
        HL_REG_CW_HANG_TIME     = 0x10,

        HL_REG_RX8_NCO_FREQ     = 0x12,
        HL_REG_RX9_NCO_FREQ     = 0x13,
        HL_REG_RX10_NCO_FREQ    = 0x14,
        HL_REG_RX11_NCO_FREQ    = 0x15,
        HL_REG_RX12_NCO_FREQ    = 0x16,

        HL_REG_PREDISTORTION   = 0x2B,

        HL_REG_RESET_ON_DCNT   = 0x3A,
        HL_REG_AD9866_SPI      = 0x3B,
        HL_REG_I2C_1           = 0x3C,
        HL_REG_I2C_2           = 0x3D,
        HL_REG_ERROR           = 0x3F,
    };

    enum HermesLiteSamplerate {
        HL_SAMP_RATE_48KHZ  = 0,
        HL_SAMP_RATE_96KHZ  = 1,
        HL_SAMP_RATE_192KHZ = 2,
        HL_SAMP_RATE_384KHZ = 3
    };

#pragma pack(push, 1)
    struct HPSDRUSBHeader {
        uint8_t sync[3];
        uint8_t c0;
        uint8_t c[4];
    };

    struct MetisPacketHeader {
        uint16_t signature;
        uint8_t type;
    };

    struct MetisUSBPacket {
        MetisPacketHeader hdr;
        uint8_t endpoint;
        uint32_t seq;
        uint8_t frame[2][512];
    };

    struct MetisControlPacket {
        MetisPacketHeader hdr;
        uint8_t ctrl;
        uint8_t rsvd[60];
    };

    // struct MetisDiscoverPacket {
    //     MetisPacketHeader hdr;
    //     union {
    //         {
    //             uint16_t mac[6];
    //             uint8_t gatewareMajVer;
    //             uint8_t boardId;
    //         };
    //         uint8_t rsvd[60];
    //     };
    // };
#pragma pack(pop)

    class Client {
    public:
        Client(std::shared_ptr<net::Socket> sock);

        void close();

        void start();
        void stop();

        void setFrequency(double freq);
        void setGain(int gain);

        dsp::stream<dsp::complex_t> out;

    //private:
        void sendMetisUSB(uint8_t endpoint, void* frame0, void* frame1 = NULL);
        void sendMetisControl(MetisControl ctrl);

        uint32_t readReg(uint8_t addr);
        void writeReg(uint8_t addr, uint32_t val); 

        void worker();

        bool open = true;

        std::thread workerThread;
        std::shared_ptr<net::Socket> sock;
        uint32_t usbSeq = 0;

    };

    std::vector<Info> discover();
    std::shared_ptr<Client> open(std::string host, int port);
}