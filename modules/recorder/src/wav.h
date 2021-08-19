#pragma once
#include <stdint.h>
#include <fstream>

#define WAV_SIGNATURE       "RIFF"
#define WAV_TYPE            "WAVE"
#define WAV_FORMAT_MARK     "fmt "
#define WAV_DATA_MARK       "data"
#define WAV_SAMPLE_TYPE_PCM 1

class WavWriter {
public:
    WavWriter(std::string path, uint16_t bitDepth, uint16_t channelCount, uint32_t sampleRate) {
        file = std::ofstream(path.c_str(), std::ios::binary);
        memcpy(hdr.signature, WAV_SIGNATURE, 4);
        memcpy(hdr.fileType, WAV_TYPE, 4);
        memcpy(hdr.formatMarker, WAV_FORMAT_MARK, 4);
        memcpy(hdr.dataMarker, WAV_DATA_MARK, 4);
        hdr.formatHeaderLength = 16;
        hdr.sampleType = WAV_SAMPLE_TYPE_PCM;
        hdr.channelCount = channelCount;
        hdr.sampleRate = sampleRate;
        hdr.bytesPerSecond = (bitDepth / 8) * channelCount * sampleRate;
        hdr.bytesPerSample = (bitDepth / 8) * channelCount;
        hdr.bitDepth = bitDepth;
        file.write((char*)&hdr, sizeof(WavHeader_t));
    }

    bool isOpen() {
        return file.is_open();
    }

    void writeSamples(void* data, size_t size) {
        file.write((char*)data, size);
        bytesWritten += size;
    }
    
    void close() {
        hdr.fileSize = bytesWritten + sizeof(WavHeader_t) - 8;
        hdr.dataSize = bytesWritten;
        file.seekp(0);
        file.write((char*)&hdr, sizeof(WavHeader_t));
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

    std::ofstream file;
    size_t bytesWritten = 0;
    WavHeader_t hdr;
};