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


#define RFSPACE_16BIT_SAMPLES_PER_PKT 256

class RFSpaceClient {
public:
  RFSpaceClient() :
    ctrlsock(INVALID_SOCKET),
    datasock(INVALID_SOCKET),
    rxaddr_size(0)
  {
    memset(&rxaddr, 0, sizeof(rxaddr));
  }

  enum RFSpaceVersionReq {
    BOOT_CODE_VER = 0,
    LOADED_FW_VER = 1,
    HARDWARE_VER  = 2,
    FPGA_CFG_VER  = 3,
    ROM_0_VER     = 4,
    ROM_1_VER     = 5,
    ROM_2_VER     = 6,
    FPGA_CFG1_VER = 7,
    FPGA_CFG2_VER = 8   // FIXME through CFG 32
  };

  enum RFSpaceStatusError {
    IDLE                        = 0x0b,
    RUNNING                     = 0x0c,
    BOOT_MODE_IDLE              = 0x0e,
    BOOT_MODE_PROGRAMMING       = 0x0f,
    SIGNAL_OVERLOAD             = 0x20,
    BOOT_MODE_PROGRAMMING_ERROR = 0x80
  };

  enum RFSpaceRFInput {
    RF_AUTO_SELECT  = 0x00,
    RF_PORT_1       = 0x01,
    RF_PORT_2       = 0x02
  };

  enum RFSpaceRFGain {
    RF_GAIN_0_DB        = 0x00,
    RF_GAIN_MINUS_10_DB = 0xf6,
    RF_GAIN_MINUS_20_DB = 0xec,
    RF_GAIN_MINUS_30_DB = 0xe2
  };

  enum RFSpaceFilter {
    FLT_AUTO_SELECT               = 0x00,
    FLT_2MHZ_HP                   = 0x01,
    FLT_2MHZ_LP                   = 0x02,
    FLT_2MHZ_HP_20MHZ_LP          = 0x03,
    FLT_20MHZ_HP                  = 0x04,
    FLT_2MHZ_HP_20MHZ_HP          = 0x05,
    FLT_20MHZ_LP_20MHZ_HP         = 0x06,
    FLT_2MHZ_HP_20MHZ_LP_20MHZ_HP = 0x07,
    FLT_BYPASS                    = 0x08
  };

  enum RFSpaceADMode {
    AD_GAIN_0       = 0x00,
    AD_GAIN_1_DOT_5 = 0x01
  };

  enum RFSpaceInputSync {
    SYNC_NO_HW_SYNC        = 0x00,
    SYNC_NEG_EDGE_START    = 0x01,
    SYNC_POS_EDGE_START    = 0x02,
    SYNC_LOW_LEVEL_START   = 0x03,
    SYNC_HIGH_LEVEL_START  = 0x04,
    SYNC_NOTUSED5          = 0x05,
    SYNC_NOTUSED6          = 0x06,
    SYNC_INTERNAL_TRIGGER  = 0x07
  };

  enum RFSpaceOutputPktSize {
    LARGE_UDP = 0x00,    // (1444 bytes(24-bit data) or 1028 bytes(16-bit data) ) default
    SMALL_UDP = 0x01     // (388 bytes(24-bit data) or 516 bytes(16-bit data) )
  };

  _declspec(align(2)) struct RFSpaceDataHdr {
    uint16_t Type;
    uint16_t SequenceNum;

    RFSpaceDataHdr() :
      Type(0), SequenceNum(0)
    {}
  };

  _declspec(align(2)) struct RFSpaceIQSample {
    uint16_t I;
    uint16_t Q;

    RFSpaceIQSample() :
      I(0), Q(0)
    {}
  };

  _declspec(align(2)) struct RFSpaceIQDataPkt {
    RFSpaceDataHdr Header;
    RFSpaceIQSample Samples[RFSPACE_16BIT_SAMPLES_PER_PKT];
  };

  bool connectToRFSpace(char* host, uint16_t port) {
    if (connected) {
      return true;
    }

#ifdef _WIN32
    struct addrinfo* result = NULL;
    struct addrinfo* ptr = NULL;
    struct addrinfo hints;

    WSADATA wsaData;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (rc != 0) {
      return false;
    }

    ZeroMemory(&hints, sizeof(hints));
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

    ctrlsock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

    if (ctrlsock == INVALID_SOCKET) {
      // TODO: log error
      printf("B");
      freeaddrinfo(result);
      WSACleanup();
      return false;
    }

    //DWORD ctrltimeout = 1000; // mSec
    //setsockopt(ctrlsock, SOL_SOCKET, SO_RCVTIMEO, (char*)&ctrltimeout, sizeof(ctrltimeout));

    iResult = connect(ctrlsock, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
      printf("C");
      closesocket(ctrlsock);
      freeaddrinfo(result);
      WSACleanup();
      return false;
    }
    freeaddrinfo(result);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    iResult = getaddrinfo(host, buf, &hints, &result);
    if (iResult != 0) {
      // TODO: log error
      printf("\n%s\n", gai_strerror(iResult));
      WSACleanup();
      return false;
    }
    ptr = result;

    datasock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

    if (datasock == INVALID_SOCKET) {
      // TODO: log error
      printf("D");
      freeaddrinfo(result);
      WSACleanup();
      return false;
    }

    DWORD datatimeout = 2 * 1000;
    setsockopt(datasock, SOL_SOCKET, SO_RCVTIMEO, (char*)&datatimeout, sizeof(datatimeout));

    memset(&rxaddr, 0, sizeof(rxaddr));
    rxaddr.sin_family = AF_INET;
    printf("port = %d\n", port);
    rxaddr.sin_port = htons(port);
    rxaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    rxaddr_size = sizeof(rxaddr);

    iResult = bind(datasock, (sockaddr*)&rxaddr, sizeof(rxaddr));
    if (iResult != 0) {
      // TODO: log error
      printf("E");
      freeaddrinfo(result);
      WSACleanup();
      return false;
    }
#else
    ctrlsockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctrlsockfd < 0) {
      // TODO: Log error
      return false;
    }
    struct hostent* server = gethostbyname(host);
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(ctrlsockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
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
    closesocket(datasock);
    closesocket(ctrlsock);
    WSACleanup();
#else
    close(ctrlsockfd);
#endif
    connected = false;
  }

  void sendCommand(const uint8_t* cmd, const int len) {
#ifdef _WIN32
    int rc = send(ctrlsock, (char*)cmd, len, 0);
    if (SOCKET_ERROR != 0)
    {
      int err = WSAGetLastError();
    }
    ::Sleep(50);  // FIXME
#else
    (void)write(ctrlsockfd, &cmd, len);
#endif
  }

  void receiveData(uint8_t* buf, size_t count, size_t& bytesrecd) {
    bytesrecd = 0;
    int received = 0;
    int ret = 0;

    while (received < count) {
#ifdef _WIN32
      ret = recv(datasock, (char*)&inPkt, inPktSize, 0);
      if (ret != SOCKET_ERROR) {
        memcpy(&buf[received], &inPkt.Samples[0], sampleSize);
      }
      else {
        int err = WSAGetLastError();
        if (WSAETIMEDOUT == err) {
          bytesrecd = 0;
          return;
        }
      }
#else
      ret = read(datasockfd, &buf[received], count - received);
#endif
      if (ret <= 0) {
        bytesrecd = 0;
        return;
      }
      received += sampleSize;
    }

    bytesrecd = received;
  }

  void receiveResp(uint8_t* buf, size_t count) {
    memset(buf, 0, count);
    int received = 0;
    int ret = 0;
    while (received < count) {
#ifdef _WIN32
      ret = recv(ctrlsock, (char*)&buf[received], count - received, 0);
#else
      ret = read(ctrlsockfd, &buf[received], count - received);
#endif
      if (ret <= 0) { return; }
      received += ret;
    }
  }

  void getRadioName() {
    constexpr uint8_t cmd[] = { 0x04, 0x20, 0x01, 0x00 };
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void getRadioSerialNum() {
    constexpr uint8_t cmd[] = { 0x04, 0x20, 0x02, 0x00 };
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void getInterfaceVersion() {
    constexpr uint8_t cmd[] = { 0x04, 0x20, 0x03, 0x00 };
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void getVersion(const RFSpaceVersionReq req) {
    uint8_t cmd[] = { 0x05, 0x20, 0x04, 0x00, 0x01 };
    cmd[4] = req;
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void getStatusErrCode() {
    constexpr uint8_t cmd[] = { 0x04, 0x20, 0x05, 0x00 };
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void getCustomName() {
    constexpr uint8_t cmd[] = { 0x04, 0x20, 0x08, 0x00 };
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void getProductId() {
    constexpr uint8_t cmd[] = { 0x04, 0x20, 0x09, 0x00 };
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void getOptions() {
    constexpr uint8_t cmd[] = { 0x04, 0x20, 0x0a, 0x00 };
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void setFrequency(const double freq) {
    uint8_t cmd[] = { 0x0a, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const uint64_t f = static_cast<uint64_t>(freq);
    memcpy(&cmd[5], &f, 5);  // 5 byte frequency value in Hz
    sendCommand(&cmd[0], sizeof(cmd));
    cmd[4] = 0x01;
    sendCommand(&cmd[0], sizeof(cmd)); // send it on channel 1 ???
  }

  void setSampleRate(double sr) {
    uint8_t cmd[] = { 0x09, 0x00, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const uint32_t r = static_cast<uint32_t>(sr);
    memcpy(&cmd[5], &r, 4);
    sendCommand((uint8_t*)&cmd, sizeof(cmd));
  }

  void setRfFilterSelection(RFSpaceFilter flt) {
    uint8_t cmd[] = { 0x06, 0x00, 0x44, 0x00, 0x00, 0x00 };
    cmd[5] = flt;
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void setRfGain(RFSpaceRFGain gain) {
    uint8_t cmd[] = { 0x06, 0x00, 0x38, 0x00, 0x00, 0x00 };
    cmd[5] = gain;
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void setAtoDMode(RFSpaceADMode mode) {
    uint8_t cmd[] = { 0x06, 0x00, 0x8a, 0x00, 0x00, 0x00 };
    cmd[5] = mode;
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void setGainIndex(int index) {
    //sendCommand(13, index);
  }

  void setInputSyncMode(RFSpaceInputSync sync, uint16_t kpktsToSend) {
    uint8_t cmd[] = { 0x08, 0x00, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x00 };
    cmd[5] = sync;

    switch (sync)
    {
      NEG_EDGE_START:
      POS_EDGE_START:
      LOW_LEVEL_START:
      HIGH_LEVEL_START:
        cmd[6] = kpktsToSend & 0xff;          // LSB
        cmd[7] = (kpktsToSend & 0xff00) >> 8; // MSB
        break;

      default:
        break;
    }

    sendCommand(&cmd[0], sizeof(cmd));
  }

  void setStart(void) {
    //constexpr uint8_t cmd[] = { 0x08, 0x00, 0x18, 0x00, 0x00, 0x02, 0x00, 0x00 }; // 16-bit contiguous real
    constexpr uint8_t cmd[] = { 0x08, 0x00, 0x18, 0x00, 0x80, 0x02, 0x00, 0x00 }; // 16-bit contiguous IQ
    //constexpr uint8_t cmd[] = { 0x08, 0x00, 0x18, 0x00, 0x80, 0x02, 0x80, 0x00 }; // 24-bit contiguous IQ
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void setStop(void) {
    constexpr uint8_t cmd[] = { 0x06, 0x00, 0x18, 0x00, 0x00, 0x01 };
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void getSecurityCode(void) {
    constexpr uint8_t cmd[] = { 0x05, 0x20, 0xb0, 0x00, 0x00 };
    sendCommand(&cmd[0], sizeof(cmd));
  }

  void setRFInput(RFSpaceRFInput input) {
    uint8_t cmd[] = { 0x06, 0x00, 0x30, 0x00, 0x00, 0x00 };
    cmd[5] = input;
    sendCommand(&cmd[0], sizeof(cmd));
  }

private:
#ifdef _WIN32
    struct sockaddr_in rxaddr;
    int rxaddr_size;
    SOCKET ctrlsock;
    SOCKET datasock;
#else
    int ctrlsockfd;
    int datasockfd;
#endif

    bool connected = false;
    const size_t sampleSize = sizeof(RFSpaceIQDataPkt::Samples);  // 256 I and 256 Q int16_t
    RFSpaceIQDataPkt inPkt;
    const size_t inPktSize = sizeof(RFSpaceIQDataPkt);
};
