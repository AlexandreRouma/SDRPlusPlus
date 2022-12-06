#include "riff.h"
#include <stdexcept>

namespace riff {
    bool Writer::open(std::string path, char form[4]) {
        // TODO:  Open file

        beginRIFF(form);

        return false;
    }

    bool Writer::isOpen() {
        
        return false;
    }

    void Writer::close() {
        if (!isOpen()) { return; }

        endRIFF();

        file.close();
    }

    void Writer::beginChunk(char id[4]) {
        // Create and write header
        ChunkDesc desc;
        desc.pos = file.tellp();
        memcpy(desc.hdr.id, id, sizeof(desc.hdr.id));
        desc.hdr.size = 0;
        file.write((char*)&desc.hdr, sizeof(ChunkHeader));

        // Save descriptor
        chunks.push(desc);
    }

    void Writer::endChunk() {
        if (!chunks.empty()) {
            throw std::runtime_error("No chunk to end");
        }

        // Get descriptor
        ChunkDesc desc = chunks.top();
        chunks.pop();

        // Write size
        auto pos = file.tellp();
        file.seekp(desc.pos + 4);
        file.write((char*)&desc.hdr.size, sizeof(desc.hdr.size));
        file.seekp(pos);

        // If parent chunk, increment its size
        if (!chunks.empty()) {
            chunks.top().hdr.size += desc.hdr.size;
        }
    }

    void Writer::write(void* data, size_t len) {
        if (!chunks.empty()) {
            throw std::runtime_error("No chunk to write into");
        }
        file.write((char*)data, len);
        chunks.top().hdr.size += len;
    }

    void Writer::beginRIFF(char form[4]) {
        if (!chunks.empty()) {
            throw std::runtime_error("Can't create RIFF chunk on an existing RIFF file");
        }

        // Create chunk with RIFF ID and write form
        beginChunk("RIFF");
        write(form, sizeof(form));
    }

    void Writer::endRIFF() {
        if (!chunks.empty()) {
            throw std::runtime_error("No chunk to end");
        }
        if (memcmp(chunks.top().hdr.id, "RIFF", 4)) {
            throw std::runtime_error("Top chunk not RIFF chunk");
        }
        endChunk();
    }
}