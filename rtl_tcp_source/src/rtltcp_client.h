#pragma once
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

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

class RTLTCPClient {
public:
    RTLTCPClient() {
        
    }

    bool connectToRTL(char* host, uint16_t port) {
        if (connected) {
            return true;
        }

#ifdef _WIN32
        struct addrinfo *result = NULL;
        struct addrinfo *ptr = NULL;
        struct addrinfo hints;

        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,2), &wsaData);

        ZeroMemory( &hints, sizeof(hints) );
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        char buf[128];
        sprintf(buf, "%hu", port);

        int iResult = getaddrinfo(host, buf, &hints, &result);
        if (iResult != 0) {
            // TODO: log error
            printf("\n%s\n", gai_strerror(iResult));
            WSACleanup();
            return false;
        }
        ptr = result;

        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

        if (sock == INVALID_SOCKET) {
            // TODO: log error
            printf("B");
            freeaddrinfo(result);
            WSACleanup();
            return false;
        }

        iResult = connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            printf("C");
            closesocket(sock);
            freeaddrinfo(result);
            WSACleanup();
            return false;
        }
        freeaddrinfo(result);
#else
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            // TODO: Log error
            return false;
        }
        struct hostent *server = gethostbyname(host);
        struct sockaddr_in serv_addr;
        bzero(&serv_addr, sizeof(struct sockaddr_in));
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(port);
        if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
            // TODO: log error
            return false;
        }
#endif

        connected = true;

        printf("Connected");

        return true;
    }

    void disconnect() {
        if (!connected) {
            return;
        }
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sockfd);
#endif
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
#ifdef _WIN32
        send(sock, (char*)&cmd, sizeof(command_t), 0);  
#else
        (void)write(sockfd, &cmd, sizeof(command_t));
#endif
    }

    void receiveData(uint8_t* buf, size_t count) {
#ifdef _WIN32
        recv(sock, (char*)buf, count, 0);
#else
        (void)read(sockfd, buf, count);
#endif
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

    void setBiasTee(bool enabled) {
        sendCommand(14, enabled);
    }

private:
#ifdef _WIN32
    SOCKET sock;
#else
    int sockfd;
#endif
    
    bool connected = false;

};