#pragma once
#include <mutex>
#include <fstream>
#include <string>
#include <stack>
#include <stdint.h>

namespace riff {
#pragma pack(push, 1)
    struct ChunkHeader {
        char id[4];
        uint32_t size;
    };
#pragma pack(pop)

    struct ChunkDesc {
        ChunkHeader hdr;
        std::streampos pos;
    };

    class Writer {
    public:
        Writer() {}
        // Writer(const Writer&& b);
        ~Writer();

        bool open(std::string path, const char form[4]);
        bool isOpen();
        void close();

        void beginList(const char id[4]);
        void endList();

        void beginChunk(const char id[4]);
        void endChunk();

        void write(const uint8_t* data, size_t len);

    private:
        void beginRIFF(const char form[4]);
        void endRIFF();

        std::recursive_mutex mtx;
        std::ofstream file;
        std::stack<ChunkDesc> chunks;
    };

    // class Reader {
    // public:
    //     Reader();
    //     Reader(const Reader&& b);
    //     ~Reader();

    //     bool open(std::string path);
    //     bool isOpen();
    //     void close();

    //     const std::string& form();

    // private:

    //     std::string _form;
    //     std::recursive_mutex mtx;
    //     std::ofstream file;
    // };
}