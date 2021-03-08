#pragma once

#pragma once
#include <stdint.h>
#include <fstream>

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

    void readSamples(void* data, size_t size) {
        file.read((char*)data, size);
        bytesRead += size;
    }
    
    void close() {
        
        file.close();
    }

private:
    struct WavHeader_t {
        char signature[4];              // "RIFF"
        uint32_t fileSize;              // data bytes + sizeof(WavHeader_t) - 8
        char fileType[4];               // "WAVE"
        char formatMarker[4];           // "fmt " 
        uint32_t formatHeaderLength;    // Always 16
        uint16_t sampleType;            // PCM (1)   
        uint16_t channelCount;
        uint32_t sampleRate;
        uint32_t bytesPerSecond;
        uint16_t bytesPerSample;
        uint16_t bitDepth;
        char dataMarker[4];             // "data"
        uint32_t dataSize;
    };

    std::ifstream file;
    size_t bytesRead = 0;
    WavHeader_t hdr;
};