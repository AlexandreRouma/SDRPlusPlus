#pragma once
#include <stdint.h>
#include <mutex>
#include <memory>

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
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#endif

namespace net {
#ifdef _WIN32
    typedef SOCKET SockHandle_t;
#else
    typedef int SockHandle_t;
#endif

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
        Socket(SockHandle_t sock, struct sockaddr_in* raddr = NULL);
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
         * @return Number of bytes sent.
         */
        int send(const uint8_t* data, size_t len);

        /**
         * Send string on socket. Terminating null byte is not sent, include one in the string if you need it.
         * @param str String to be sent.
         * @return Number of bytes sent.
         */
        int sendstr(const std::string& str);

        /**
         * Receive data from socket.
         * @param data Buffer to read the data into.
         * @param maxLen Maximum number of bytes to read.
         * @param forceLen Read the maximum number of bytes even if it requires multiple receive operations.
         * @param timeout Timeout in milliseconds. Use NO_TIMEOUT or NONBLOCKING here if needed.
         * @return Number of bytes read. 0 means timed out or closed. -1 means would block or error.
         */
        int recv(uint8_t* data, size_t maxLen, bool forceLen = false, int timeout = NO_TIMEOUT);

        /**
         * Receive line from socket.
         * @param str String to read the data into.
         * @param maxLen Maximum line length allowed, 0 for no limit.
         * @param timeout Timeout in milliseconds.  Use NO_TIMEOUT or NONBLOCKING here if needed.
         * @return Length of the returned string. 0 means timed out or closed. -1 means would block or error.
         */
        int recvline(std::string& str, int maxLen = 0, int timeout = NO_TIMEOUT);

    private:
        struct sockaddr_in* raddr = NULL;
        SockHandle_t sock;
        bool open = true;

    };

    class Listener {
    public:
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
         * @param timeout Timeout in milliseconds. 0 means no timeout.
         * @return Socket of the connection. NULL means timed out or closed.
         */
        std::shared_ptr<Socket> accept(int timeout = NO_TIMEOUT);

    private:
        SockHandle_t sock;
        bool open = true;

    };

    /**
     * Create TCP listener.
     * @param host Hostname or IP to listen on ("0.0.0.0" for Any).
     * @param port Port to listen on.
     * @return Listener instance on success, null otherwise.
     */
    std::shared_ptr<Listener> listen(std::string host, int port);

    /**
     * Create TCP connection.
     * @param host Remote hostname or IP address.
     * @param port Remote port.
     * @return Socket instance on success, null otherwise.
     */
    std::shared_ptr<Socket> connect(std::string host, int port);

    /**
     * Create UDP socket.
     * @param rhost Remote hostname or IP address.
     * @param rport Remote port.
     * @param lhost Local hostname or IP used to bind the socket (optional, "0.0.0.0" for Any).
     * @param lpost Local port used to bind the socket (optional, 0 to allocate automatically).
     * @return Socket instance on success, null otherwise.
     */
    std::shared_ptr<Socket> openudp(std::string rhost, int rport, std::string lhost = "0.0.0.0", int lport = 0);
}