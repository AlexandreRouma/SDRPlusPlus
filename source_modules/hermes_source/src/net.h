#pragma once
#include <stdint.h>
#include <mutex>
#include <memory>
#include <map>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#else
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <ifaddrs.h>
#endif

namespace net {
#ifdef _WIN32
    typedef SOCKET SockHandle_t;
    typedef int socklen_t;
#else
    typedef int SockHandle_t;
#endif
    typedef uint32_t IP_t;

    class Socket;
    class Listener;

    struct InterfaceInfo {
        IP_t address;
        IP_t netmask;
        IP_t broadcast;
    };

    class Address {
        friend Socket;
        friend Listener;
    public:
        /**
         * Default constructor. Corresponds to 0.0.0.0:0.
         */
        Address();

        /**
         * Do not instantiate this class manually. Use the provided functions.
         * @param host Hostname or IP.
         * @param port TCP/UDP port.
         */
        Address(const std::string& host, int port);

        /**
         * Do not instantiate this class manually. Use the provided functions.
         * @param ip IP in host byte order.
         * @param port TCP/UDP port.
         */
        Address(IP_t ip, int port);

        /**
         * Get the IP address.
         * @return IP address in standard string format.
         */
        std::string getIPStr();

        /**
         * Get the IP address.
         * @return IP address in host byte order.
         */
        IP_t getIP();

        /**
         * Set the IP address.
         * @param ip IP address in host byte order.
         */
        void setIP(IP_t ip);

        /**
         * Get the TCP/UDP port.
         * @return TCP/UDP port number.
         */
        int getPort();

        /**
         * Set the TCP/UDP port.
         * @param port TCP/UDP port number.
         */
        void setPort(int port);

        struct sockaddr_in addr;
    };

    enum {
        NO_TIMEOUT  = -1,
        NONBLOCKING = 0
    };

    enum SocketType {
        SOCKET_TYPE_TCP,
        SOCKET_TYPE_UDP
    };

    class Socket {
    public:
        /**
         * Do not instantiate this class manually. Use the provided functions.
         */
        Socket(SockHandle_t sock, const Address* raddr = NULL);
        ~Socket();

        /**
         * Close socket. The socket can no longer be used after this.
         */
        void close();

        /**
         * Check if the socket is open.
         * @return True if open, false if closed.
         */
        bool isOpen();

        /**
         * Get socket type. Either TCP or UDP.
         * @return Socket type.
         */
        SocketType type();

        /**
         * Send data on socket.
         * @param data Data to be sent.
         * @param len Number of bytes to be sent.
         * @param dest Destination address. NULL to use the default remote address.
         * @return Number of bytes sent.
         */
        int send(const uint8_t* data, size_t len, const Address* dest = NULL);

        /**
         * Send string on socket. Terminating NULL byte is not sent, include one in the string if you need it.
         * @param str String to be sent.
         * @param dest Destination address. NULL to use the default remote address.
         * @return Number of bytes sent.
         */
        int sendstr(const std::string& str, const Address* dest = NULL);

        /**
         * Receive data from socket.
         * @param data Buffer to read the data into.
         * @param maxLen Maximum number of bytes to read.
         * @param forceLen Read the maximum number of bytes even if it requires multiple receive operations.
         * @param timeout Timeout in milliseconds. Use NO_TIMEOUT or NONBLOCKING here if needed.
         * @param dest Destination address. If multiple packets, this will contain the address of the last one. NULL if not used.
         * @return Number of bytes read. 0 means timed out or closed. -1 means would block or error.
         */
        int recv(uint8_t* data, size_t maxLen, bool forceLen = false, int timeout = NO_TIMEOUT, Address* dest = NULL);

        /**
         * Receive line from socket.
         * @param str String to read the data into.
         * @param maxLen Maximum line length allowed, 0 for no limit.
         * @param timeout Timeout in milliseconds.  Use NO_TIMEOUT or NONBLOCKING here if needed.
         * @param dest Destination address. If multiple packets, this will contain the address of the last one. NULL if not used.
         * @return Length of the returned string. 0 means timed out or closed. -1 means would block or error.
         */
        int recvline(std::string& str, int maxLen = 0, int timeout = NO_TIMEOUT, Address* dest = NULL);

    private:
        Address* raddr = NULL;
        SockHandle_t sock;
        bool open = true;

    };

    class Listener {
    public:
        /**
         * Do not instantiate this class manually. Use the provided functions.
         */
        Listener(SockHandle_t sock);
        ~Listener();

        /**
         * Stop listening. The listener can no longer be used after this.
         */
        void stop();

        /**
         * CHeck if the listener is still listening.
         * @return True if listening, false if not.
         */
        bool listening();

        /**
         * Accept connection.
         * @param timeout Timeout in milliseconds. Use NO_TIMEOUT or NONBLOCKING here if needed.
         * @return Socket of the connection. NULL means timed out, would block or closed.
         */
        std::shared_ptr<Socket> accept(Address* dest = NULL, int timeout = NO_TIMEOUT);

    private:
        SockHandle_t sock;
        bool open = true;

    };

    /**
     * Get a list of the network interface.
     * @return List of network interfaces and their addresses.
     */
    std::map<std::string, InterfaceInfo> listInterfaces();

    /**
     * Create TCP listener.
     * @param addr Address to listen on.
     * @return Listener instance on success, Throws runtime_error otherwise.
     */
    std::shared_ptr<Listener> listen(const Address& addr);

    /**
     * Create TCP listener.
     * @param host Hostname or IP to listen on ("0.0.0.0" for Any).
     * @param port Port to listen on.
     * @return Listener instance on success, Throws runtime_error otherwise.
     */
    std::shared_ptr<Listener> listen(std::string host, int port);

    /**
     * Create TCP connection.
     * @param addr Remote address.
     * @return Socket instance on success, Throws runtime_error otherwise.
     */
    std::shared_ptr<Socket> connect(const Address& addr);  

    /**
     * Create TCP connection.
     * @param host Remote hostname or IP address.
     * @param port Remote port.
     * @return Socket instance on success, Throws runtime_error otherwise.
     */
    std::shared_ptr<Socket> connect(std::string host, int port);  

    /**
     * Create UDP socket.
     * @param raddr Remote address.
     * @param laddr Local address to bind the socket to.
     * @return Socket instance on success, Throws runtime_error otherwise.
     */
    std::shared_ptr<Socket> openudp(const Address& raddr, const Address& laddr);

    /**
     * Create UDP socket.
     * @param rhost Remote hostname or IP address.
     * @param rport Remote port.
     * @param laddr Local address to bind the socket to.
     * @return Socket instance on success, Throws runtime_error otherwise.
     */
    std::shared_ptr<Socket> openudp(std::string rhost, int rport, const Address& laddr);

    /**
     * Create UDP socket.
     * @param raddr Remote address.
     * @param lhost Local hostname or IP used to bind the socket (optional, "0.0.0.0" for Any).
     * @param lpost Local port used to bind the socket to (optional, 0 to allocate automatically).
     * @return Socket instance on success, Throws runtime_error otherwise.
     */
    std::shared_ptr<Socket> openudp(const Address& raddr, std::string lhost = "0.0.0.0", int lport = 0);

    /**
     * Create UDP socket.
     * @param rhost Remote hostname or IP address.
     * @param rport Remote port.
     * @param lhost Local hostname or IP used to bind the socket (optional, "0.0.0.0" for Any).
     * @param lpost Local port used to bind the socket to (optional, 0 to allocate automatically).
     * @return Socket instance on success, Throws runtime_error otherwise.
     */
    std::shared_ptr<Socket> openudp(std::string rhost, int rport, std::string lhost = "0.0.0.0", int lport = 0);  
}