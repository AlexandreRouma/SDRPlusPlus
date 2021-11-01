#pragma once

#pragma once
#include <stdint.h>
#include <string.h>
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
        sizeWavHeader = sizeof(WavHeader_t);
        file.read((char*)&hdr, sizeof(WavHeader_t));
        valid = false;
        if (memcmp(hdr.signature, "RIFF", 4) != 0) { return; }
        if (memcmp(hdr.fileType, "WAVE", 4) != 0) { return; }

        if (memcmp(hdr.dataMarker, "data", 4) == 0) {
            valid = true;
        }
        else {
            WavHeaderNonPCM_t hdrNonPCM = {0};
            memcpy(&hdrNonPCM, &hdr, sizeof(WavHeader_t));
            file.read(((char*)&hdrNonPCM)+sizeof(WavHeader_t), sizeof(WavHeaderNonPCM_t)-sizeof(WavHeader_t));
            sizeWavHeader = sizeof(WavHeaderNonPCM_t);
            if (memcmp(hdrNonPCM.dataMarker, "data", 4) == 0) valid = true;
        }
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
            file.seekg(sizeWavHeader);
            file.read(&_data[read], size - read);
        }
        bytesRead += size;
    }

    void rewind() {
        file.seekg(sizeWavHeader);
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

    struct __attribute__((__packed__)) WavHeaderNonPCM_t {
        char signature[4];              // "RIFF"
        uint32_t fileSize;              // data bytes + sizeof(WavHeaderNonPCM_t) - 8
        char fileType[4];               // "WAVE"
        char formatMarker[4];           // "fmt "
        uint32_t formatHeaderLength;    // 18
        uint16_t sampleType;            // WAVE_FORMAT_IEEE_FLOAT=3
        uint16_t channelCount;
        uint32_t sampleRate;
        uint32_t bytesPerSecond;
        uint16_t bytesPerSample;
        uint16_t bitDepth;
        uint16_t extSize;               // 0
        char factMarker[4];             // "fact"
        uint32_t _ckSize4;
        uint32_t _dwSampleLen;
        char dataMarker[4];             // "data"
        uint32_t dataSize;
    };

    bool valid = false;
    std::ifstream file;
    size_t bytesRead = 0;
    size_t sizeWavHeader = 0;
    WavHeader_t hdr;
};

