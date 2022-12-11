#pragma once
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <windows.h>
#define INVSOC INVALID_SOCKET
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifndef SOCKET
#define SOCKET int
#define INVSOC (-1)
#endif
#endif

namespace redpitaya {
    enum RedpitayaSamplerate {
        REDPITAYA_SAMP_RATE_20KHZ  = 0,
        REDPITAYA_SAMP_RATE_50KHZ  = 1,
        REDPITAYA_SAMP_RATE_100KHZ = 2,
        REDPITAYA_SAMP_RATE_250KHZ = 3,
        REDPITAYA_SAMP_RATE_500KHZ = 4,
        REDPITAYA_SAMP_RATE_1250KHZ = 5
    };

    const double sampleRate[] = 
    {
        20000.0,
        50000.0,
        100000.0,
        250000.0,
        500000.0,
        1250000.0
    };

    SOCKET _sockets[2];
}