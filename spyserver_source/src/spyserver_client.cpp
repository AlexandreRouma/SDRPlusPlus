#include <spyserver_client.h>
#include <spdlog/spdlog.h>

SpyServerClient::SpyServerClient() {

}

bool SpyServerClient::connectToSpyserver(char* host, int port) {
    if (connected) {
        return true;
    }

#ifdef _WIN32
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
        printf("A");
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

    // Switch to non-blocking mode
#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
#endif

    connected = true;
    waiting = true;

    workerThread = std::thread(&SpyServerClient::worker, this);

    printf("Connected");

    return true;
}

bool SpyServerClient::disconnect() {
        if (!connected) {
            return false;
        }
        waiting = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sockfd);
#endif
        connected = false;
        return true;
}

void SpyServerClient::setSampleRate(uint32_t setSampleRate) {

}

void SpyServerClient::tune(uint32_t freq) {

}

int SpyServerClient::receive(char* buf, int count) {
#ifdef _WIN32
    return checkError(recv(sock, (char*)buf, count, 0), count);
#else
    return checkError(read(sockfd, buf, count), count);
#endif
}

int SpyServerClient::checkError(int len, int expected) {
#ifdef _WIN32
    if (len != SOCKET_ERROR) { return len; }
    int code = WSAGetLastError();
    if (code == WSAEWOULDBLOCK) { return 0; }
    spdlog::error("{0}", code);
    return -1;
#else
    if (len <= expected) {
        return len;
    }
    if (len == EAGAIN || len == EWOULDBLOCK) { return 0; }
    return -1;
#endif  
}

int SpyServerClient::receiveSync(char* buf, int count) {
    int len = receive(buf, count);
    while (len == 0 && waiting) {
        len = receive(buf, count);
    }
    if (!waiting) {
        return 0;
    }
    return len;
}

void SpyServerClient::worker() {
    // Allocate dummy buffer
    char* dummyBuf = (char*)malloc(1000000);

    // Send hello
    hello();

    //   SETTING_STREAMING_MODE        = 0,
    //   SETTING_STREAMING_ENABLED     = 1,
    //   SETTING_GAIN                  = 2,

    //   SETTING_IQ_FORMAT             = 100,    // 0x64
    //   SETTING_IQ_FREQUENCY          = 101,    // 0x65
    //   SETTING_IQ_DECIMATION         = 102,    // 0x66
    //   SETTING_IQ_DIGITAL_GAIN       = 103,    // 0x67

    // Set settings
    setSetting(SETTING_STREAMING_MODE, STREAM_MODE_IQ_ONLY);
    setSetting(SETTING_GAIN, 5);
    setSetting(SETTING_IQ_FORMAT, STREAM_FORMAT_FLOAT);
    setSetting(SETTING_IQ_FREQUENCY, 2000000);
    setSetting(SETTING_IQ_DECIMATION, 1);
    setSetting(SETTING_IQ_DIGITAL_GAIN, 1);

    // Enable stream
    setSetting(SETTING_STREAMING_ENABLED, 1);

    // Main message receive loop
    while (true) {
        MessageHeader msgHeader;
        int len = receiveSync((char*)&msgHeader, sizeof(MessageHeader));
        if (len < 0) {
            spdlog::error("Socket error");
            return;
        }
        if (len == 0) { return; }

        int type = (msgHeader.MessageType & 0xFFFF);

        if (type == MSG_TYPE_DEVICE_INFO) {
            DeviceInfo devInfo;
            len = receiveSync((char*)&devInfo, sizeof(DeviceInfo));
            if (len < 0) {
                spdlog::error("A Socket error");
                return;
            }
            if (len == 0) { return; }

            spdlog::warn("Dev type: {0}", devInfo.DeviceType);
        }
        // else if (type == MSG_TYPE_FLOAT_IQ) {
        //     //if (iqStream.aquire() < 0) { return; }
        //     len = receiveSync(dummyBuf, msgHeader.BodySize);
        //     //iqStream.write(msgHeader.BodySize);
        //     if (len < 0) {
        //         spdlog::error("T Socket error");
        //         return;
        //     }
        //     if (len == 0) { return; }
        // }   
        else if (msgHeader.BodySize > 0) {
            len = receiveSync(dummyBuf, msgHeader.BodySize);
            if (len < 0) {
                spdlog::error("B Socket error {0}", msgHeader.ProtocolID);
                return;
            }
            if (len == 0) { return; }
        }
    }

    free(dummyBuf);
}

void SpyServerClient::sendCommand(uint32_t cmd, void* body, size_t bodySize) {
    int size = sizeof(CommandHeader) + bodySize;
    char* buf = new char[size];
    CommandHeader* cmdHdr = (CommandHeader*)buf;
    memcpy(&buf[sizeof(CommandHeader)], body, bodySize);
    cmdHdr->CommandType = cmd;
    cmdHdr->BodySize = bodySize;
#ifdef _WIN32
    send(sock, buf, size, 0);
#else
    write(sockfd, buf, size);
#endif
    delete[] buf;
}

void SpyServerClient::setSetting(uint32_t setting, uint32_t value) {
    char buf[sizeof(SettingTarget) + sizeof(uint32_t)];
    SettingTarget* tgt = (SettingTarget*)buf;
    uint32_t* val = (uint32_t*)&buf[sizeof(SettingTarget)];
    tgt->SettingType = setting;
    *val = value;
    sendCommand(CMD_SET_SETTING, buf, sizeof(SettingTarget) + sizeof(uint32_t));
}

void SpyServerClient::hello() {
    char buf[1024];
    ClientHandshake* handshake = (ClientHandshake*)buf;
    handshake->ProtocolVersion = SPYSERVER_PROTOCOL_VERSION;
    strcpy(&buf[sizeof(ClientHandshake)], "sdr++");
    sendCommand(CMD_HELLO, buf, sizeof(ClientHandshake) + 5);
}