#pragma once
#include <utils/networking.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <atomic>

#define RFSPACE_MAX_SIZE 8192

namespace rfspace {
    enum {
        RFSPACE_MSG_TYPE_H2T_SET_CTRL_ITEM,
        RFSPACE_MSG_TYPE_H2T_REQ_CTRL_ITEM,
        RFSPACE_MSG_TYPE_H2T_REQ_CTRL_ITEM_RANGE,
        RFSPACE_MSG_TYPE_H2T_DATA_ITEM_ACK_REQ,
        RFSPACE_MSG_TYPE_H2T_DATA_ITEM_0,
        RFSPACE_MSG_TYPE_H2T_DATA_ITEM_1,
        RFSPACE_MSG_TYPE_H2T_DATA_ITEM_2,
        RFSPACE_MSG_TYPE_H2T_DATA_ITEM_3,
    };

    enum {
        RFSPACE_MSG_TYPE_T2H_SET_CTRL_ITEM_RESP,
        RFSPACE_MSG_TYPE_T2H_UNSOL_CTRL_ITEM,
        RFSPACE_MSG_TYPE_T2H_REQ_CTRL_ITEM_RANGE_RESP,
        RFSPACE_MSG_TYPE_T2H_DATA_ITEM_ACK_REQ,
        RFSPACE_MSG_TYPE_T2H_DATA_ITEM_0,
        RFSPACE_MSG_TYPE_T2H_DATA_ITEM_1,
        RFSPACE_MSG_TYPE_T2H_DATA_ITEM_2,
        RFSPACE_MSG_TYPE_T2H_DATA_ITEM_3,
    };

    class SyncEvent {
    public:
        bool await(int timeoutMS) {
            // Mark as waiting
            {
                std::lock_guard<std::mutex> lck(waitingmtx);
                waiting = true;
            }

            // Wait for data
            std::unique_lock<std::mutex> lck(mtx);
            return cnd.wait_for(lck, std::chrono::milliseconds(timeoutMS), [this]() { return triggered; });

            // Mark as not waiting
            {
                std::lock_guard<std::mutex> lck(waitingmtx);
                waiting = false;
            }
        }

        void release() {
            // Mark as not waiting, and if last notify sender
            {
                std::lock_guard<std::mutex> lck(mtx);
                triggered = false;
            }
            cnd.notify_all();
        }

        void trigger() {
            // Check if a waiter is waiting
            {
                std::lock_guard<std::mutex> lck(waitingmtx);
                if (waiting <= 0) { return; }
            }

            // Notify waiters
            {
                std::lock_guard<std::mutex> lck(mtx);
                triggered = true;
            }
            cnd.notify_all();

            // Wait for waiter to handle
            {
                std::unique_lock<std::mutex> lck(mtx);
                cnd.wait(lck, [this]() { return !triggered; });
            }
        }

    private:
        bool triggered;
        std::mutex mtx;
        std::condition_variable cnd;

        bool waiting;
        std::mutex waitingmtx;
    };

    class RFspaceClientClass {
    public:
        RFspaceClientClass(net::Conn conn, net::Conn udpConn, dsp::stream<dsp::complex_t>* out);
        ~RFspaceClientClass();

        int getControlItem(uint16_t item, void* param, int len);
        void setControlItem(uint16_t item, void* param, int len);

        void close();
        bool isOpen();

    private:
        void sendCommand(uint32_t command, void* data, int len);
        void sendHandshake(std::string appName);

        static void tcpHandler(int count, uint8_t* buf, void* ctx);
        static void udpHandler(int count, uint8_t* buf, void* ctx);

        net::Conn client;
        net::Conn udpClient;

        dsp::stream<dsp::complex_t>* output;

        uint16_t tcpHeader;
        uint16_t udpHeader;

        uint8_t* rbuffer = NULL;
        uint8_t* sbuffer = NULL;
        uint8_t* ubuffer = NULL;

        SyncEvent SCIRRecv;
        int SCIRSize;
    };

    typedef std::unique_ptr<RFspaceClientClass> RFspaceClient;

    RFspaceClient connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* out);

}
