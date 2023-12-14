#include "riff.h"
#include <string.h>
#include <stdexcept>

namespace riff {
    const char* RIFF_SIGNATURE      = "RIFF";
    const char* LIST_SIGNATURE      = "LIST";
    const size_t RIFF_LABEL_SIZE    = 4;

    bool Writer::open(std::string path, const char form[4]) {
        std::lock_guard<std::recursive_mutex> lck(mtx);

        // Open file
        file = std::ofstream(path, std::ios::out | std::ios::binary);
        if (!file.is_open()) { return false; }

        // Begin RIFF chunk
        beginRIFF(form);

        return true;
    }

    bool Writer::isOpen() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        return file.is_open();
    }

    void Writer::close() {
        std::lock_guard<std::recursive_mutex> lck(mtx);

        if (!isOpen()) { return; }

        // Finalize RIFF chunk
        endRIFF();

        // Close file
        file.close();
    }

    void Writer::beginList(const char id[4]) {
        std::lock_guard<std::recursive_mutex> lck(mtx);

        // Create chunk with the LIST ID and write id
        beginChunk(LIST_SIGNATURE);
        write((uint8_t*)id, RIFF_LABEL_SIZE);
    }

    void Writer::endList() {
        std::lock_guard<std::recursive_mutex> lck(mtx);

        if (chunks.empty()) {
            throw std::runtime_error("No chunk to end");
        }
        if (memcmp(chunks.top().hdr.id, LIST_SIGNATURE, RIFF_LABEL_SIZE)) {
            throw std::runtime_error("Top chunk not LIST chunk");
        }

        endChunk();
    }

    void Writer::beginChunk(const char id[4]) {
        std::lock_guard<std::recursive_mutex> lck(mtx);

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
        std::lock_guard<std::recursive_mutex> lck(mtx);

        if (chunks.empty()) {
            throw std::runtime_error("No chunk to end");
        }

        // Get descriptor
        ChunkDesc desc = chunks.top();
        chunks.pop();

        // Write size
        auto pos = file.tellp();
        auto npos = desc.pos;
        npos += 4;
        file.seekp(npos);
        file.write((char*)&desc.hdr.size, sizeof(desc.hdr.size));
        file.seekp(pos);

        // If parent chunk, increment its size by the size of the sub-chunk plus the size of its header)
        if (!chunks.empty()) {
            chunks.top().hdr.size += desc.hdr.size + sizeof(ChunkHeader);
        }
    }

    void Writer::write(const uint8_t* data, size_t len) {
        std::lock_guard<std::recursive_mutex> lck(mtx);

        if (chunks.empty()) {
            throw std::runtime_error("No chunk to write into");
        }
        file.write((char*)data, len);
        chunks.top().hdr.size += len;
    }

    void Writer::beginRIFF(const char form[4]) {
        std::lock_guard<std::recursive_mutex> lck(mtx);

        if (!chunks.empty()) {
            throw std::runtime_error("Can't create RIFF chunk on an existing RIFF file");
        }

        // Create chunk with RIFF ID and write form
        beginChunk(RIFF_SIGNATURE);
        write((uint8_t*)form, RIFF_LABEL_SIZE);
    }

    void Writer::endRIFF() {
        std::lock_guard<std::recursive_mutex> lck(mtx);

        if (chunks.empty()) {
            throw std::runtime_error("No chunk to end");
        }
        if (memcmp(chunks.top().hdr.id, RIFF_SIGNATURE, RIFF_LABEL_SIZE)) {
            throw std::runtime_error("Top chunk not RIFF chunk");
        }

        endChunk();
    }
}