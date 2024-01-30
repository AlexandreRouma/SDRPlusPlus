#include "net.h"
#include <string.h>
#include <codecvt>
#include <stdexcept>

#ifdef _WIN32
#define WOULD_BLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#else
#define WOULD_BLOCK (errno == EWOULDBLOCK)
#endif

namespace net {
    bool _init = false;
    
    // === Private functions ===

    void init() {
        if (_init) { return; }
#ifdef _WIN32
        // Initialize WinSock2
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
            throw std::runtime_error("Could not initialize WinSock2");
            return;
        }
#else
        // Disable SIGPIPE to avoid closing when the remote host disconnects
        signal(SIGPIPE, SIG_IGN);
#endif
        _init = true;
    }

    bool queryHost(uint32_t* addr, std::string host) {
        hostent* ent = gethostbyname(host.c_str());
        if (!ent || !ent->h_addr_list[0]) { return false; }
        *addr = *(uint32_t*)ent->h_addr_list[0];
        return true;
    }

    void closeSocket(SockHandle_t sock) {
#ifdef _WIN32
        shutdown(sock, SD_BOTH);
        closesocket(sock);
#else
        shutdown(sock, SHUT_RDWR);
        close(sock);
#endif
    }

    void setNonblocking(SockHandle_t sock) {
#ifdef _WIN32
        u_long enabled = 1;
        ioctlsocket(sock, FIONBIO, &enabled);
#else
        fcntl(sock, F_SETFL, O_NONBLOCK);
#endif
    }

    // === Address functions ===

    Address::Address() {
        memset(&addr, 0, sizeof(addr));
    }

    Address::Address(const std::string& host, int port) {
        // Initialize WSA if needed
        init();
        
        // Lookup host
        hostent* ent = gethostbyname(host.c_str());
        if (!ent || !ent->h_addr_list[0]) {
            throw std::runtime_error("Unknown host");
        }
        
        // Build address
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = *(uint32_t*)ent->h_addr_list[0];
        addr.sin_port = htons(port);
    }

    Address::Address(IP_t ip, int port) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(ip);
        addr.sin_port = htons(port);
    }

    std::string Address::getIPStr() const {
        char buf[128];
        IP_t ip = getIP();
        sprintf(buf, "%d.%d.%d.%d", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
        return buf;
    }

    IP_t Address::getIP() const {
        return htonl(addr.sin_addr.s_addr);
    }

    void Address::setIP(IP_t ip) {
        addr.sin_addr.s_addr = htonl(ip);
    }

    int Address::getPort() const {
        return htons(addr.sin_port);
    }

    void Address::setPort(int port) {
        addr.sin_port = htons(port);
    }

    // === Socket functions ===

    Socket::Socket(SockHandle_t sock, const Address* raddr) {
        this->sock = sock;
        if (raddr) {
            this->raddr = new Address(*raddr);
        }
    }

    Socket::~Socket() {
        close();
        if (raddr) { delete raddr; }
    }

    void Socket::close() {
        if (!open) { return; }
        open = false;
        closeSocket(sock);
    }

    bool Socket::isOpen() {
        return open;
    }

    SocketType Socket::type() {
        return raddr ? SOCKET_TYPE_UDP : SOCKET_TYPE_TCP;
    }

    int Socket::send(const uint8_t* data, size_t len, const Address* dest) {
        return sendto(sock, (const char*)data, len, 0, (sockaddr*)(dest ? &dest->addr : (raddr ? &raddr->addr : NULL)), sizeof(sockaddr_in));
    }

    int Socket::sendstr(const std::string& str, const Address* dest) {
        return send((const uint8_t*)str.c_str(), str.length(), dest);
    }

    int Socket::recv(uint8_t* data, size_t maxLen, bool forceLen, int timeout, Address* dest) {
        // Create FD set
        fd_set set;
        FD_ZERO(&set);
        
        int read = 0;
        bool blocking = (timeout != NONBLOCKING);
        do {
            // Wait for data or error if 
            if (blocking) {
                // Enable FD in set
                FD_SET(sock, &set);

                // Set timeout
                timeval tv;
                tv.tv_sec = timeout / 1000;
                tv.tv_usec = (timeout - tv.tv_sec*1000) * 1000;

                // Wait for data
                int err = select(sock+1, &set, NULL, &set, (timeout > 0) ? &tv : NULL);
                if (err <= 0) { return err; }
            }

            // Receive
            int addrLen = sizeof(sockaddr_in);
            int err = ::recvfrom(sock, (char*)&data[read], maxLen - read, 0,(sockaddr*)(dest ? &dest->addr : NULL), (socklen_t*)(dest ? &addrLen : NULL));
            if (err <= 0 && !WOULD_BLOCK) {
                close();
                return err;
            }
            read += err;
        }
        while (blocking && forceLen && read < maxLen);
        return read;
    }

    int Socket::recvline(std::string& str, int maxLen, int timeout, Address* dest) {
        // Disallow nonblocking mode
        if (!timeout) { return -1; }
        
        str.clear();
        int read = 0;
        while (!maxLen || read < maxLen) {
            char c;
            int err = recv((uint8_t*)&c, 1, false, timeout, dest);
            if (err <= 0) { return err; }
            read++;
            if (c == '\n') { break; }
            str += c;
        }
        return read;
    }

    // === Listener functions ===

    Listener::Listener(SockHandle_t sock) {
        this->sock = sock;
    }

    Listener::~Listener() {
        stop();
    }

    void Listener::stop() {
        closeSocket(sock);
        open = false;
    }

    bool Listener::listening() {
        return open;
    }

    std::shared_ptr<Socket> Listener::accept(Address* dest, int timeout) {
        // Create FD set
        fd_set set;
        FD_ZERO(&set);
        FD_SET(sock, &set);

        // Define timeout
        timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout - tv.tv_sec*1000) * 1000;

        // Wait for data or error
        if (timeout != NONBLOCKING) {
            int err = select(sock+1, &set, NULL, &set, (timeout > 0) ? &tv : NULL);
            if (err <= 0) { return NULL; }
        }

        // Accept
        int addrLen = sizeof(sockaddr_in);
        SockHandle_t s = ::accept(sock, (sockaddr*)(dest ? &dest->addr : NULL), (socklen_t*)(dest ? &addrLen : NULL));
        if ((int)s < 0) {
            if (!WOULD_BLOCK) { stop(); }
            return NULL;
        }

        // Enable nonblocking mode
        setNonblocking(s);

        return std::make_shared<Socket>(s);
    }

    // === Creation functions ===

    std::map<std::string, InterfaceInfo> listInterfaces() {
        // Init library if needed
        init();

        std::map<std::string, InterfaceInfo> ifaces;
#ifdef _WIN32
        // Pre-allocate buffer
        ULONG size = sizeof(IP_ADAPTER_ADDRESSES);
        PIP_ADAPTER_ADDRESSES addresses = (PIP_ADAPTER_ADDRESSES)malloc(size);

        // Reallocate to real size
        if (GetAdaptersAddresses(AF_INET, 0, NULL, addresses, &size) == ERROR_BUFFER_OVERFLOW) {
            addresses = (PIP_ADAPTER_ADDRESSES)realloc(addresses, size);
            if (GetAdaptersAddresses(AF_INET, 0, NULL, addresses, &size)) {
                throw std::exception("Could not list network interfaces");
            }
        }

        // Save data
        std::wstring_convert<std::codecvt_utf8<wchar_t>> utfConv;
        for (auto iface = addresses; iface; iface = iface->Next) {
            InterfaceInfo info;
            auto ip = iface->FirstUnicastAddress;
            if (!ip || ip->Address.lpSockaddr->sa_family != AF_INET) { continue; }
            info.address = ntohl(*(uint32_t*)&ip->Address.lpSockaddr->sa_data[2]);
            info.netmask = ~((1 << (32 - ip->OnLinkPrefixLength)) - 1);
            info.broadcast = info.address | (~info.netmask);
            ifaces[utfConv.to_bytes(iface->FriendlyName)] = info;
        }
        
        // Free tables
        free(addresses);
#else
        // Get iface list
        struct ifaddrs* addresses = NULL;
        getifaddrs(&addresses);

        // Save data
        for (auto iface = addresses; iface; iface = iface->ifa_next) {
            if (!iface->ifa_addr || !iface->ifa_netmask) { continue; }
            if (iface->ifa_addr->sa_family != AF_INET) { continue; }
            InterfaceInfo info;
            info.address = ntohl(*(uint32_t*)&iface->ifa_addr->sa_data[2]);
            info.netmask = ntohl(*(uint32_t*)&iface->ifa_netmask->sa_data[2]);
            info.broadcast = info.address | (~info.netmask);
            ifaces[iface->ifa_name] = info;
        }

        // Free iface list
        freeifaddrs(addresses);
#endif

        return ifaces;
    }

    std::shared_ptr<Listener> listen(const Address& addr) {
        // Init library if needed
        init();

        // Create socket
        SockHandle_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        // TODO: Support non-blockign mode

#ifndef _WIN32
        // Allow port reusing if the app was killed or crashed
        // and the socket is stuck in TIME_WAIT state.
        // This option has a different meaning on Windows,
        // so we use it only for non-Windows systems
        int enable = 1;
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            closeSocket(s);
            throw std::runtime_error("Could not configure socket");
            return NULL;
        }
#endif

        // Bind socket to the port
        if (bind(s, (sockaddr*)&addr.addr, sizeof(sockaddr_in))) {
            closeSocket(s);
            throw std::runtime_error("Could not bind socket");
            return NULL;
        }

        // Enable listening
        if (::listen(s, SOMAXCONN) != 0) {
            throw std::runtime_error("Could start listening for connections");
            return NULL;
        }

        // Enable nonblocking mode
        setNonblocking(s);

        // Return listener class
        return std::make_shared<Listener>(s);
    }

    std::shared_ptr<Listener> listen(std::string host, int port) {
        return listen(Address(host, port));
    }

    std::shared_ptr<Socket> connect(const Address& addr) {
        // Init library if needed
        init();

        // Create socket
        SockHandle_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        // Connect to server
        if (::connect(s, (sockaddr*)&addr.addr, sizeof(sockaddr_in))) {
            closeSocket(s);
            throw std::runtime_error("Could not connect");
            return NULL;
        }

        // Enable nonblocking mode
        setNonblocking(s);

        // Return socket class
        return std::make_shared<Socket>(s);
    }

    std::shared_ptr<Socket> connect(std::string host, int port) {
        return connect(Address(host, port));
    }

    std::shared_ptr<Socket> openudp(const Address& raddr, const Address& laddr, bool allowBroadcast) {
        // Init library if needed
        init();

        // Create socket
        SockHandle_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        // If the remote address is multicast, allow multicast connections
        #ifdef _WIN32
                const char enable = allowBroadcast;
        #else
                int enable = allowBroadcast;
        #endif
        if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int)) < 0) {
            closeSocket(s);
            throw std::runtime_error("Could not enable broadcast on socket");
            return NULL;
        }

        // Bind socket to local port
        if (bind(s, (sockaddr*)&laddr.addr, sizeof(sockaddr_in))) {
            closeSocket(s);
            throw std::runtime_error("Could not bind socket");
            return NULL;
        }
        
        // Return socket class
        return std::make_shared<Socket>(s, &raddr);
    }

    std::shared_ptr<Socket> openudp(std::string rhost, int rport, const Address& laddr, bool allowBroadcast) {
        return openudp(Address(rhost, rport), laddr, allowBroadcast);
    }

    std::shared_ptr<Socket> openudp(const Address& raddr, std::string lhost, int lport, bool allowBroadcast) {
        return openudp(raddr, Address(lhost, lport), allowBroadcast);
    }

    std::shared_ptr<Socket> openudp(std::string rhost, int rport, std::string lhost, int lport, bool allowBroadcast) {
        return openudp(Address(rhost, rport), Address(lhost, lport), allowBroadcast);
    }
}
