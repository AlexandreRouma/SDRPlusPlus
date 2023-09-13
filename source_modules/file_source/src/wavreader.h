#pragma once

#pragma once
#include <stdint.h>
#include <string.h>
#include <fstream>
#include <iostream>

#define WAV_SIGNATURE       "RIFF"
#define WAV_TYPE            "WAVE"
#define WAV_FORMAT_MARK     "fmt "
#define WAV_DATA_MARK       "data"
#define WAV_SAMPLE_TYPE_PCM 1

class WavReader {
public:
    WavReader(std::string path) {
        file = std::ifstream(path.c_str(), std::ios::binary);
        file.read((char*)&hdr, sizeof(WavHeader_t));
        valid = false;
        if (memcmp(hdr.signature, "RIFF", 4) != 0) { return; }
        if (memcmp(hdr.fileType, "WAVE", 4) != 0) { return; }

        file.seekg(0, std::ios::end);
        lengthBytes = (uint64_t)file.tellg() - (uint64_t)sizeof(WavHeader_t);
        lengthSeconds = lengthBytes / hdr.bytesPerSecond;

        rewind();

        valid = true;
    }

    uint16_t getBitDepth() {
        return hdr.bitDepth;
    }

    uint16_t getChannelCount() {
        return hdr.channelCount;
    }

    uint32_t getSampleRate() {
        return hdr.sampleRate;
    }

    bool isValid() {
        return valid;
    }

    void readSamples(void* data, size_t size) {
        char* _data = (char*)data;
        file.read(_data, size);
        int read = file.gcount();
        if (read < size) {
            file.clear();
            file.seekg(sizeof(WavHeader_t));
            file.read(&_data[read], size - read);
        }
        bytesRead += size;
    }

    void rewind() {
        file.seekg(sizeof(WavHeader_t));
    }

    auto getLength() {
        return lengthSeconds;
    }

    uint32_t getPosition() {
        return (uint32_t)(((uint64_t)file.tellg() - (uint64_t)sizeof(WavHeader_t)) / hdr.bytesPerSecond);
    }

    void rewindBySeconds(int32_t seconds) {
        int64_t rewindBytes = ((int32_t)hdr.bytesPerSecond) * seconds;
        int64_t curPos = file.tellg();
        int64_t newPos = curPos + rewindBytes;

        if (newPos > (lengthBytes + sizeof(WavHeader_t))) {
            newPos = lengthBytes + sizeof(WavHeader_t);
        } else if (newPos < sizeof(WavHeader_t)) {
            newPos = sizeof(WavHeader_t);
        }

        std::cout << "Rewind by seconds. newPos: " << newPos << ", rewindByts: " << rewindBytes << std::endl;

        file.seekg(newPos);
    }

    void close() {
        file.close();
    }

private:
    struct WavHeader_t {
        char signature[4];           // "RIFF"
        uint32_t fileSize;           // data bytes + sizeof(WavHeader_t) - 8
        char fileType[4];            // "WAVE"
        char formatMarker[4];        // "fmt "
        uint32_t formatHeaderLength; // Always 16
        uint16_t sampleType;         // PCM (1)
        uint16_t channelCount;
        uint32_t sampleRate;
        uint32_t bytesPerSecond;
        uint16_t bytesPerSample;
        uint16_t bitDepth;
        char dataMarker[4]; // "data"
        uint32_t dataSize;
    };

    bool valid = false;
    std::ifstream file;
    size_t bytesRead = 0;
    WavHeader_t hdr;
    uint32_t lengthSeconds = 0;
    uint64_t lengthBytes = 0;
};