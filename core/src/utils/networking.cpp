#include <utils/networking.h>
#include <assert.h>
#include <spdlog/spdlog.h>

namespace net {

#ifdef _WIN32
    extern bool winsock_init = false;
#endif

    ConnClass::ConnClass(Socket sock, struct sockaddr_in raddr, bool udp) {
        _sock = sock;
        _udp = udp;
        remoteAddr = raddr;
        connectionOpen = true;
        readWorkerThread = std::thread(&ConnClass::readWorker, this);
        writeWorkerThread = std::thread(&ConnClass::writeWorker, this);
    }

    ConnClass::~ConnClass() {
        ConnClass::close();
    }

    void ConnClass::close() {
        std::lock_guard lck(closeMtx);
        // Set stopWorkers to true
        {
            std::lock_guard lck1(readQueueMtx);
            std::lock_guard lck2(writeQueueMtx);
            stopWorkers = true;
        }

        // Notify the workers of the change
        readQueueCnd.notify_all();
        writeQueueCnd.notify_all();

        if (connectionOpen) {
#ifdef _WIN32
            closesocket(_sock);
#else
            ::shutdown(_sock, SHUT_RDWR);
            ::close(_sock);
#endif
        }

        // Wait for the theads to terminate
        if (readWorkerThread.joinable()) { readWorkerThread.join(); }
        if (writeWorkerThread.joinable()) { writeWorkerThread.join(); }

        {
            std::lock_guard lck(connectionOpenMtx);
            connectionOpen = false;
        }
        connectionOpenCnd.notify_all();
    }

    bool ConnClass::isOpen() {
        return connectionOpen;
    }

    void ConnClass::waitForEnd() {
        std::unique_lock lck(readQueueMtx);
        connectionOpenCnd.wait(lck, [this]() { return !connectionOpen; });
    }

    int ConnClass::read(int count, uint8_t* buf, bool enforceSize) {
        if (!connectionOpen) { return -1; }
        std::lock_guard lck(readMtx);
        int ret;

        if (_udp) {
            socklen_t fromLen = sizeof(remoteAddr);
            ret = recvfrom(_sock, (char*)buf, count, 0, (struct sockaddr*)&remoteAddr, &fromLen);
            if (ret <= 0) {
                {
                    std::lock_guard lck(connectionOpenMtx);
                    connectionOpen = false;
                }
                connectionOpenCnd.notify_all();
                return -1;
            }
            return count;
        }

        int beenRead = 0;
        while (beenRead < count) {
            ret = recv(_sock, (char*)&buf[beenRead], count - beenRead, 0);

            if (ret <= 0) {
                {
                    std::lock_guard lck(connectionOpenMtx);
                    connectionOpen = false;
                }
                connectionOpenCnd.notify_all();
                return -1;
            }

            if (!enforceSize) { return ret; }

            beenRead += ret;
        }

        return beenRead;
    }

    bool ConnClass::write(int count, uint8_t* buf) {
        if (!connectionOpen) { return false; }
        std::lock_guard lck(writeMtx);
        int ret;

        if (_udp) {
            ret = sendto(_sock, (char*)buf, count, 0, (struct sockaddr*)&remoteAddr, sizeof(remoteAddr));
            if (ret <= 0) {
                {
                    std::lock_guard lck(connectionOpenMtx);
                    connectionOpen = false;
                }
                connectionOpenCnd.notify_all();
            }
            return (ret > 0);
        }

        int beenWritten = 0;
        while (beenWritten < count) {
            ret = send(_sock, (char*)buf, count, 0);
            if (ret <= 0) {
                {
                    std::lock_guard lck(connectionOpenMtx);
                    connectionOpen = false;
                }
                connectionOpenCnd.notify_all();
                return false;
            }
            beenWritten += ret;
        }
        
        return true;
    }

    void ConnClass::readAsync(int count, uint8_t* buf, void (*handler)(int count, uint8_t* buf, void* ctx), void* ctx, bool enforceSize) {
        if (!connectionOpen) { return; }
        // Create entry
        ConnReadEntry entry;
        entry.count = count;
        entry.buf = buf;
        entry.handler = handler;
        entry.ctx = ctx;
        entry.enforceSize = enforceSize;

        // Add entry to queue
        {
            std::lock_guard lck(readQueueMtx);
            readQueue.push_back(entry);
        }

        // Notify read worker
        readQueueCnd.notify_all();
    }

    void ConnClass::writeAsync(int count, uint8_t* buf) {
        if (!connectionOpen) { return; }
        // Create entry
        ConnWriteEntry entry;
        entry.count = count;
        entry.buf = buf;

        // Add entry to queue
        {
            std::lock_guard lck(writeQueueMtx);
            writeQueue.push_back(entry);
        }

        // Notify write worker
        writeQueueCnd.notify_all();
    }

    void ConnClass::readWorker() {
        while (true) {
            // Wait for wakeup and exit if it's for terminating the thread
            std::unique_lock lck(readQueueMtx);
            readQueueCnd.wait(lck, [this]() { return (readQueue.size() > 0 || stopWorkers); });
            if (stopWorkers || !connectionOpen) { return; }

            // Pop first element off the list
            ConnReadEntry entry = readQueue[0];
            readQueue.erase(readQueue.begin());
            lck.unlock();

            // Read from socket and send data to the handler
            int ret = read(entry.count, entry.buf, entry.enforceSize);
            if (ret <= 0) {
                {
                    std::lock_guard lck(connectionOpenMtx);
                    connectionOpen = false;
                }
                connectionOpenCnd.notify_all();
                return;
            }
            entry.handler(ret, entry.buf, entry.ctx);
        }
    }

    void ConnClass::writeWorker() {
        while (true) {
            // Wait for wakeup and exit if it's for terminating the thread
            std::unique_lock lck(writeQueueMtx);
            writeQueueCnd.wait(lck, [this]() { return (writeQueue.size() > 0 || stopWorkers); });
            if (stopWorkers || !connectionOpen) { return; }

            // Pop first element off the list
            ConnWriteEntry entry = writeQueue[0];
            writeQueue.erase(writeQueue.begin());
            lck.unlock();

            // Write to socket
            if (!write(entry.count, entry.buf)) {
                {
                    std::lock_guard lck(connectionOpenMtx);
                    connectionOpen = false;
                }
                connectionOpenCnd.notify_all();
                return;
            }
        }
    }


    ListenerClass::ListenerClass(Socket listenSock) {
        sock = listenSock;
        listening = true;
        acceptWorkerThread = std::thread(&ListenerClass::worker, this);
    }

    ListenerClass::~ListenerClass() {
        close();
    }

    Conn ListenerClass::accept() {
        if (!listening) { return NULL; }
        std::lock_guard lck(acceptMtx);
        Socket _sock;

        // Accept socket
        _sock = ::accept(sock, NULL, NULL);
#ifdef _WIN32
        if (_sock < 0 || _sock == SOCKET_ERROR) {
#else
        if (_sock < 0) {
#endif
            listening = false;
            throw std::runtime_error("Could not bind socket");
            return NULL;
        }

        return Conn(new ConnClass(_sock));
    }

    void ListenerClass::acceptAsync(void (*handler)(Conn conn, void* ctx), void* ctx) {
        if (!listening) { return; }
        // Create entry
        ListenerAcceptEntry entry;
        entry.handler = handler;
        entry.ctx = ctx;

        // Add entry to queue
        {
            std::lock_guard lck(acceptQueueMtx);
            acceptQueue.push_back(entry);
        }

        // Notify write worker
        acceptQueueCnd.notify_all();
    }

    void ListenerClass::close() {
        {
            std::lock_guard lck(acceptQueueMtx);
            stopWorker = true;
        }
        acceptQueueCnd.notify_all();

        if (listening) {
#ifdef _WIN32
            closesocket(sock);
#else
            ::shutdown(sock, SHUT_RDWR);
            ::close(sock);
#endif
        }

        if (acceptWorkerThread.joinable()) { acceptWorkerThread.join(); }


        listening = false;
    }

    bool ListenerClass::isListening() {
        return listening;
    }

    void ListenerClass::worker() {
        while (true) {
            // Wait for wakeup and exit if it's for terminating the thread
            std::unique_lock lck(acceptQueueMtx);
            acceptQueueCnd.wait(lck, [this]() { return (acceptQueue.size() > 0 || stopWorker); });
            if (stopWorker || !listening) { return; }

            // Pop first element off the list
            ListenerAcceptEntry entry = acceptQueue[0];
            acceptQueue.erase(acceptQueue.begin());
            lck.unlock();

            // Read from socket and send data to the handler
            try {
                Conn client = accept();
                if (!client) {
                    listening = false;
                    return;
                }
                entry.handler(std::move(client), entry.ctx);
            }
            catch (std::exception e) {
                listening = false;
                return;
            }
        }
    }


    Conn connect(std::string host, uint16_t port) {
        Socket sock;

#ifdef _WIN32
        // Initialize WinSock2
        if (!winsock_init) {
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
                throw std::runtime_error("Could not initialize WinSock2");
                return NULL;
            }
            winsock_init = true;
        }
        assert(winsock_init);
#else
        signal(SIGPIPE, SIG_IGN);
#endif

        // Create a socket
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) {
            throw std::runtime_error("Could not create socket");
            return NULL;
        }

        // Get address from hostname/ip
        hostent* remoteHost = gethostbyname(host.c_str());
        if (remoteHost == NULL || remoteHost->h_addr_list[0] == NULL) {
            throw std::runtime_error("Could get address from host");
            return NULL;
        }
        uint32_t* naddr = (uint32_t*)remoteHost->h_addr_list[0];

        // Create host address
        struct sockaddr_in addr;
        addr.sin_addr.s_addr = *naddr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        // Connect to host
        if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("Could not connect to host");
            return NULL;
        }

        return Conn(new ConnClass(sock));
    }

    Listener listen(std::string host, uint16_t port) {
        Socket listenSock;

#ifdef _WIN32
        // Initialize WinSock2
        if (!winsock_init) {
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
                throw std::runtime_error("Could not initialize WinSock2");
                return NULL;
            }
            winsock_init = true;
        }
        assert(winsock_init);
#else
        signal(SIGPIPE, SIG_IGN);
#endif

        // Create a socket
        listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSock < 0) {
            throw std::runtime_error("Could not create socket");
            return NULL;
        }

#ifndef _WIN32
        // Allow port reusing if the app was killed or crashed
        // and the socket is stuck in TIME_WAIT state.
        // This option has a different meaning on Windows,
        // so we use it only for non-Windows systems
        int enable = 1;
        if (setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            throw std::runtime_error("Could not configure socket");
            return NULL;
        }
#endif

        // Get address from hostname/ip
        hostent* remoteHost = gethostbyname(host.c_str());
        if (remoteHost == NULL || remoteHost->h_addr_list[0] == NULL) {
            throw std::runtime_error("Could get address from host");
            return NULL;
        }
        uint32_t* naddr = (uint32_t*)remoteHost->h_addr_list[0];

        // Create host address
        struct sockaddr_in addr;
        addr.sin_addr.s_addr = *naddr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        // Bind socket
        if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("Could not bind socket");
            return NULL;
        }

        // Listen
        if (::listen(listenSock, SOMAXCONN) != 0) {
            throw std::runtime_error("Could not listen");
            return NULL;
        }

        return Listener(new ListenerClass(listenSock));
    }

    Conn openUDP(std::string host, uint16_t port, std::string remoteHost, uint16_t remotePort, bool bindSocket) {
        Socket sock;

#ifdef _WIN32
        // Initialize WinSock2
        if (!winsock_init) {
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
                throw std::runtime_error("Could not initialize WinSock2");
                return NULL;
            }
            winsock_init = true;
        }
        assert(winsock_init);
#else
        signal(SIGPIPE, SIG_IGN);
#endif

        // Create a socket
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock < 0) {
            throw std::runtime_error("Could not create socket");
            return NULL;
        }

        // Get address from local hostname/ip
        hostent* _host = gethostbyname(host.c_str());
        if (_host == NULL || _host->h_addr_list[0] == NULL) {
            throw std::runtime_error("Could get address from host");
            return NULL;
        }

        // Get address from remote hostname/ip
        hostent* _remoteHost = gethostbyname(remoteHost.c_str());
        if (_remoteHost == NULL || _remoteHost->h_addr_list[0] == NULL) {
            throw std::runtime_error("Could get address from host");
            return NULL;
        }
        uint32_t* rnaddr = (uint32_t*)_remoteHost->h_addr_list[0];

        // Create host address
        struct sockaddr_in addr;
        addr.sin_addr.s_addr = INADDR_ANY; //*naddr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        // Create remote host address
        struct sockaddr_in raddr;
        raddr.sin_addr.s_addr = *rnaddr;
        raddr.sin_family = AF_INET;
        raddr.sin_port = htons(remotePort);

        // Bind socket
        if (bindSocket) {
            int err = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
            if (err < 0) {
                throw std::runtime_error("Could not bind socket");
                return NULL;
            }
        }

        return Conn(new ConnClass(sock, raddr, true));
    }
}