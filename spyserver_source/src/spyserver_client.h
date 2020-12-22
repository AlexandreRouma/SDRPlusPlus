#pragma once
#include <spyserver_protocol.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <thread>
#include <string>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#endif

#ifdef _WIN32
#define __attribute__(x)
#pragma pack(push, 1)
#endif
struct command_t{
	unsigned char cmd;
	unsigned int param;
}__attribute__((packed));
#ifdef _WIN32
#pragma pack(pop)
#endif

class SpyServerClient {
public:
    SpyServerClient();

    bool connectToSpyserver(char* host, int port);
    bool disconnect();

    void start();
    void stop();

    void setSampleRate(uint32_t setSampleRate);

    void tune(uint32_t freq);

    dsp::stream<dsp::complex_t> iqStream;

private:
    int receive(char* buf, int count);
    int receiveSync(char* buf, int count);
    int checkError(int err, int expected);
    void worker();

    void sendCommand(uint32_t cmd, void* body, size_t bodySize);
    void setSetting(uint32_t setting, uint32_t value);

    void hello();

#ifdef _WIN32
    SOCKET sock;
#else
    int sockfd;
#endif
    bool connected = false;
    bool waiting = false;

    std::thread workerThread;

};