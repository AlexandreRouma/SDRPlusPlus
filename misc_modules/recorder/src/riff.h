#pragma once
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
        bool open(std::string path, char form[4]);
        bool isOpen();
        void close();

        void beginList();
        void endList();

        void beginChunk(char id[4]);
        void endChunk();

        void write(void* data, size_t len);

    private:
        void beginRIFF(char form[4]);
        void endRIFF();

        std::ofstream file;
        std::stack<ChunkDesc> chunks;
    };
}