#pragma once
#include <utils/networking.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <atomic>
#include <queue>

#define RFSPACE_MAX_SIZE                8192
#define RFSPACE_HEARTBEAT_INTERVAL_MS   1000
#define RFSPACE_TIMEOUT_MS              3000

namespace rfspace {
    enum H2TMessageType {
        RFSPACE_MSG_TYPE_H2T_SET_CTRL_ITEM,
        RFSPACE_MSG_TYPE_H2T_REQ_CTRL_ITEM,
        RFSPACE_MSG_TYPE_H2T_REQ_CTRL_ITEM_RANGE,
        RFSPACE_MSG_TYPE_H2T_DATA_ITEM_ACK_REQ,
        RFSPACE_MSG_TYPE_H2T_DATA_ITEM_0,
        RFSPACE_MSG_TYPE_H2T_DATA_ITEM_1,
        RFSPACE_MSG_TYPE_H2T_DATA_ITEM_2,
        RFSPACE_MSG_TYPE_H2T_DATA_ITEM_3,
    };

    enum T2HMessageType {
        RFSPACE_MSG_TYPE_T2H_SET_CTRL_ITEM_RESP,
        RFSPACE_MSG_TYPE_T2H_UNSOL_CTRL_ITEM,
        RFSPACE_MSG_TYPE_T2H_REQ_CTRL_ITEM_RANGE_RESP,
        RFSPACE_MSG_TYPE_T2H_DATA_ITEM_ACK_REQ,
        RFSPACE_MSG_TYPE_T2H_DATA_ITEM_0,
        RFSPACE_MSG_TYPE_T2H_DATA_ITEM_1,
        RFSPACE_MSG_TYPE_T2H_DATA_ITEM_2,
        RFSPACE_MSG_TYPE_T2H_DATA_ITEM_3,
    };

    enum RFPort {
        RFSPACE_RF_PORT_AUTO,
        RFSPACE_RF_PORT_1,
        RFSPACE_RF_PORT_2
    };

    enum SampleFormat {
        RFSPACE_SAMP_FORMAT_REAL    = 0x00,
        RFSPACE_SAMP_FORMAT_COMPLEX = 0x80
    };

    enum State {
        RFSPACE_STATE_IDLE  = 1,
        RFSPACE_STATE_RUN   = 2
    };

    enum SampleDepth {
        RFSPACE_SAMP_FORMAT_16BIT   = 0x00,
        RFSPACE_SAMP_FORMAT_24BIT   = 0x80
    };

    enum DeviceID {
        RFSPACE_DEV_ID_CLOUD_SDR    = 0x44534C43,
        RFSPACE_DEV_ID_CLOUD_IQ     = 0x51494C43,
        RFSPACE_DEV_ID_NET_SDR      = 0x53445204,
        RFSPACE_DEV_ID_SDR_IP       = 0x53445203
    };

    enum ControlItem {
        RFSPACE_CTRL_ITEM_MODEL_NAME    = 0x0001,
        RFSPACE_CTRL_ITEM_SERIAL        = 0x0002,
        RFSPACE_CTRL_ITEM_IFACE_VER     = 0x0003,
        RFSPACE_CTRL_ITEM_VERSION       = 0x0004,
        RFSPACE_CTRL_ITEM_STATUS        = 0x0005,
        RFSPACE_CTRL_ITEM_DEV_NAME      = 0x0008,
        RFSPACE_CTRL_ITEM_PROD_ID       = 0x0009,
        RFSPACE_CTRL_ITEM_OPTIONS       = 0x000A,
        RFSPACE_CTRL_ITEM_SECURE_CODE   = 0x000B,
        RFSPACE_CTRL_ITEM_FPGA_CONF     = 0x000C,
        RFSPACE_CTRL_ITEM_STATE         = 0x0018,
        RFSPACE_CTRL_ITEM_NCO_FREQUENCY = 0x0020,
        RFSPACE_CTRL_ITEM_RF_PORT       = 0x0030,
        RFSPACE_CTRL_ITEM_PORT_RANGE    = 0x0032,
        RFSPACE_CTRL_ITEM_RF_GAIN       = 0x0038,
        RFSPACE_CTRL_ITEM_DOWNCONV_GAIN = 0x003A,
        RFSPACE_CTRL_ITEM_RF_FILTER     = 0x0044,
        RFSPACE_CTRL_ITEM_AD_MODE       = 0x008A,
        RFSPACE_CTRL_ITEM_SAMP_RATE_CAL = 0x00B0,
        RFSPACE_CTRL_ITEM_TRIG_FREQ     = 0x00B2,
        RFSPACE_CTRL_ITEM_TRIG_PHASE    = 0x00B3,
        RFSPACE_CTRL_ITEM_SYNC_MODE     = 0x00B4,
        RFSPACE_CTRL_ITEM_IQ_SAMP_RATE  = 0x00B8,
        RFSPACE_CTRL_ITEM_UDP_PKT_SIZE  = 0x00C4,
        RFSPACE_CTRL_ITEM_UDP_ADDR      = 0x00C5,
        RFSPACE_CTRL_ITEM_TX_STATE      = 0x0118,
        RFSPACE_CTRL_ITEM_TX_FREQUENCY  = 0x0120,
        RFSPACE_CTRL_ITEM_TX_SAMP_RATE  = 0x01B8,
        RFSPACE_CTRL_ITEM_SERIAL_OPEN   = 0x0200,
        RFSPACE_CTRL_ITEM_SERIAL_CLOSE  = 0x0201,
        RFSPACE_CTRL_ITEM_SER_BOOT_RATE = 0x0202,
        RFSPACE_CTRL_ITEM_AUX_SIG_MODE  = 0x0280,
        RFSPACE_CTRL_ITEM_ERROR_LOG     = 0x0410
    };

    class RFspaceClientClass {
    public:
        RFspaceClientClass(net::Conn conn, net::Conn udpConn, dsp::stream<dsp::complex_t>* out);
        ~RFspaceClientClass();

        void sendDummyUDP();

        int getControlItem(ControlItem item, void* param, int len);
        void setControlItem(ControlItem item, void* param, int len);
        void setControlItemWithChanID(ControlItem item, uint8_t chanId, void* param, int len);

        std::vector<uint32_t> getValidSampleRates();

        void setFrequency(uint64_t freq);
        void setPort(RFPort port);
        void setGain(int8_t gain);
        void setSampleRate(uint32_t sampleRate);
        
        void start(SampleFormat sampleFormat, SampleDepth sampleDepth);
        void stop();

        void close();
        bool isOpen();

        DeviceID deviceId;

    private:
        static void tcpHandler(int count, uint8_t* buf, void* ctx);
        static void udpHandler(int count, uint8_t* buf, void* ctx);
        void heartBeatWorker();

        net::Conn client;
        net::Conn udpClient;

        dsp::stream<dsp::complex_t>* output;

        uint16_t tcpHeader;
        uint16_t udpHeader;

        uint8_t* rbuffer = NULL;
        uint8_t* sbuffer = NULL;
        uint8_t* ubuffer = NULL;

        std::thread heartBeatThread;
        std::mutex heartBeatMtx;
        std::condition_variable heartBeatCnd;
        volatile bool stopHeartBeat = false;

        bool devIdAvailable = false;
        std::condition_variable devIdCnd;
        std::mutex devIdMtx;
    };

    typedef std::unique_ptr<RFspaceClientClass> RFspaceClient;

    RFspaceClient connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* out);

}
