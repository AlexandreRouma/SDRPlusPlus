#pragma once
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

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

class RTLTCPClient {
public:
    RTLTCPClient() {
        
    }

    bool connectToRTL(char* host, uint16_t port) {
        if (connected) {
            return true;
        }

        struct addrinfo *result = NULL;
        struct addrinfo *ptr = NULL;
        struct addrinfo hints;

        ZeroMemory( &hints, sizeof(hints) );
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        char buf[128];
        sprintf(buf, "%hu", port);

        int iResult = getaddrinfo(host, buf, &hints, &result);
        if (iResult != 0) {
            // TODO: log error
            WSACleanup();
            return false;
        }
        ptr = result;

        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

        if (sock == INVALID_SOCKET) {
            // TODO: log error
            freeaddrinfo(result);
            WSACleanup();
            return false;
        }

        iResult = connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(sock);
            freeaddrinfo(result);
            WSACleanup();
            return false;
        }
        freeaddrinfo(result);

        connected = true;

        return true;
    }

    void disconnect() {
        if (!connected) {
            return;
        }
        closesocket(sock);
        WSACleanup();
        connected = false;
    }

    // struct command_t {
    //     uint8_t cmd;
    //     uint32_t arg;
    // };

    void sendCommand(uint8_t command, uint32_t param) {
        command_t cmd;
        cmd.cmd = command;
        cmd.param = htonl(param);
        send(sock, (char*)&cmd, sizeof(command_t), 0);
    }

    void receiveData(uint8_t* buf, size_t count) {
        recv(sock, (char*)buf, count, 0);
    }

    void setFrequency(double freq) {
        sendCommand(1, freq);
    }

    void setSampleRate(double sr) {
        sendCommand(2, sr);
    }

    void setGainMode(int mode) {
        sendCommand(3, mode);
    }

    void setGain(double gain) {
        sendCommand(4, gain);
    }

    void setAGCMode(int mode) {
        sendCommand(8, mode);
    }

    void setDirectSampling(int mode) {
        sendCommand(9, mode);
    }

    void setGainIndex(int index) {
        sendCommand(13, index);
    }

private:
    SOCKET sock;
    bool connected = false;

};