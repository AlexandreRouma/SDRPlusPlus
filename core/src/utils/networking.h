#pragma once
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>

struct TCPAsyncRead {
    int timeout;
    int count;
    void (*handler)(int count, char* data, void* ctx);
    void* ctx;
};

struct TCPAsyncWrite {
    int timeoutMs;
    int count;
    char* data;
};

enum {
    NET_ERR_OK = 0,
    NET_ERR_TIMEOUT = -1,
    NET_ERR_SYSTEM = -2
};

class TCPClient {
public:
    TCPClient();
    ~TCPClient();

    bool connect(std::string host, int port);
    bool disconnect();

    int enableAsync();
    int disableAsync();

    bool isAsync();

    int read(char* data, int count, int timeout = -1);
    int write(char* data, int count, int timeout = -1);

    int asyncRead(int count, void (*handler)(int count, char* data, void* ctx), int timeout = -1);
    int asyncWrite(char* data, int count, int timeout = -1);

    bool isOpen();

    int close();

private:
    void readWorker();
    void writeWorker();

    std::mutex readQueueMtx;
    std::vector<TCPAsyncRead> readQueue;

    std::mutex writeQueueMtx;
    std::vector<TCPAsyncWrite> writeQueue;

    std::thread readWorkerThread;
    std::thread writeWorkerThread;

    bool open = false;
    bool async = false;

};