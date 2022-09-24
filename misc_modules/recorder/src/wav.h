#pragma once
#include <string>
#include <fstream>
#include <stdint.h>
#include <mutex>

namespace wav {    
    #pragma pack(push, 1)
    struct Header {
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
    #pragma pack(pop)

    enum Format {
        FORMAT_WAV,
        FORMAT_RF64
    };

    enum SampleDepth {
        SAMP_DEPTH_8BIT     = 8,
        SAMP_DEPTH_16BIT    = 16,
        SAMP_DEPTH_32BIT    = 32
    };

    class Writer {
    public:
        Writer(int channels = 2, uint64_t samplerate = 48000, Format format = FORMAT_WAV, SampleDepth depth = SAMP_DEPTH_16BIT);
        ~Writer();

        bool open(std::string path);
        bool isOpen();
        void close();

        void setChannels(int channels);
        void setSamplerate(uint64_t samplerate);
        void setFormat(Format format);
        void setSampleDepth(SampleDepth depth);

        size_t getSamplesWritten() { return samplesWritten; }

        void write(float* samples, int count);

    private:
        void finalize();
        
        std::recursive_mutex mtx;
        std::ofstream file;
        Header hdr;
        bool _isOpen = false;

        int _channels;
        uint64_t _samplerate;
        Format _format;
        SampleDepth _depth;
        size_t bytesPerSamp;

        int8_t* buf8;
        int16_t* buf16;
        int32_t* buf32;
        size_t samplesWritten = 0;
    };
}
