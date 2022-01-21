#pragma once
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

/*
    Ryzerth's Epic Networking Functins
*/

namespace net {
    enum SocketType {
        SOCK_TYPE_TCP,
        SOCK_TYPE_UDP
    };

    struct ReadHandler {
        int count;
        void* buf;
        void (*handle)(int count, void* buf, void* ctx);
        void* ctx;
    };

    class SocketClass {
    public:
        SocketClass(struct addrinfo* localAddr, struct addrinfo* remoteAddr, SocketType sockType);
        ~SocketClass();

        int read(int count, void* buf, int timeout);
        int write(int count, void* buf);

        void readAsync(int count, void* buf, void (*handle)(int count, void* buf, void* ctx), void* ctx);

        bool isOpen();
        void close();

    private:
        void readWorker();

        bool open = false;

        struct addrinfo* laddr;
        struct addrinfo* raddr;
        SocketType type;

        std::queue<ReadHandler> readQueue;
        std::thread readThread;
        std::mutex readMtx;
        std::condition_variable readCnd;

    };

    typedef std::shared_ptr<SocketClass> Socket;

    namespace tcp {
        struct AcceptHandler {
            void (*handle)(Socket client, void* ctx);
            void* ctx;
        };

        class ListenerClass {
        public:
            ListenerClass(struct addrinfo* addr);
            ~ListenerClass();

            Socket accept(int count, void* buf, int timeout);
            void acceptAsync(void (*handle)(int count, void* buf, void* ctx), void* ctx);

            bool isOpen();
            void close();

        private:
            void acceptWorker();

            bool open = false;

            struct addrinfo* addr;

            std::queue<AcceptHandler> acceptQueue;
            std::thread acceptThread;
            std::mutex acceptMtx;
            std::condition_variable acceptCnd;

        };

        typedef std::shared_ptr<ListenerClass> Listener;

        Socket connect(std::string host, int port);
        Listener listen(std::string host, int port);
    }

    namespace udp {
        Socket open(std::string remoteHost, int remotePort, std::string localHost = "", int localPort = -1);
    }
}