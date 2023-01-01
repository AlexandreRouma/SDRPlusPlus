#include "wav.h"
#include <volk/volk.h>
#include <stdexcept>
#include <dsp/buffer/buffer.h>
#include <dsp/stream.h>
#include <map>

namespace wav {
    const char* WAVE_FILE_TYPE          = "WAVE";
    const char* FORMAT_MARKER           = "fmt ";
    const char* DATA_MARKER             = "data";
    const uint32_t FORMAT_HEADER_LEN    = 16;
    const uint16_t SAMPLE_TYPE_PCM      = 1;

    std::map<SampleType, int> SAMP_BITS = {
        { SAMP_TYPE_UINT8, 8 },
        { SAMP_TYPE_INT16, 16 },
        { SAMP_TYPE_INT32, 32 },
        { SAMP_TYPE_FLOAT32, 32 }
    };
    
    Writer::Writer(int channels, uint64_t samplerate, Format format, SampleType type) {
        // Validate channels and samplerate
        if (channels < 1) { throw std::runtime_error("Channel count must be greater or equal to 1"); }
        if (!samplerate) { throw std::runtime_error("Samplerate must be non-zero"); }
        
        // Initialize variables
        _channels = channels;
        _samplerate = samplerate;
        _format = format;
        _type = type;
    }

    Writer::~Writer() { close(); }

    bool Writer::open(std::string path) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Close previous file
        if (rw.isOpen()) { close(); }

        // Reset work values
        samplesWritten = 0;

        // Fill header
        bytesPerSamp = (SAMP_BITS[_type] / 8) * _channels;
        hdr.codec = (_type == SAMP_TYPE_FLOAT32) ? CODEC_FLOAT : CODEC_PCM;
        hdr.channelCount = _channels;
        hdr.sampleRate = _samplerate;
        hdr.bitDepth = SAMP_BITS[_type];
        hdr.bytesPerSample = bytesPerSamp;
        hdr.bytesPerSecond = bytesPerSamp * _samplerate;

        // Precompute sizes and allocate buffers
        switch (_type) {
        case SAMP_TYPE_UINT8:
            bufU8 = dsp::buffer::alloc<uint8_t>(STREAM_BUFFER_SIZE * _channels);
            break;
        case SAMP_TYPE_INT16:
            bufI16 = dsp::buffer::alloc<int16_t>(STREAM_BUFFER_SIZE * _channels);
            break;
        case SAMP_TYPE_INT32:
            bufI32 = dsp::buffer::alloc<int32_t>(STREAM_BUFFER_SIZE * _channels);
            break;
        case SAMP_TYPE_FLOAT32:
            break;
        default:
            return false;
            break;
        }

        // Open file
        if (!rw.open(path, WAVE_FILE_TYPE)) { return false; }

        // Write format chunk
        rw.beginChunk(FORMAT_MARKER);
        rw.write((uint8_t*)&hdr, sizeof(FormatHeader));
        rw.endChunk();

        // Begin data chunk
        rw.beginChunk(DATA_MARKER);
        
        return true;
    }

    bool Writer::isOpen() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        return rw.isOpen();
    }

    void Writer::close() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Do nothing if the file is not open
        if (!rw.isOpen()) { return; }

        // Finish data chunk
        rw.endChunk();

        // Close the file
        rw.close();

        // Free buffers
        if (bufU8) {
            dsp::buffer::free(bufU8);
            bufU8 = NULL;
        }
        if (bufI16) {
            dsp::buffer::free(bufI16);
            bufI16 = NULL;
        }
        if (bufI32) {
            dsp::buffer::free(bufI32);
            bufI32 = NULL;
        }
    }

    void Writer::setChannels(int channels) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Do not allow settings to change while open
        if (rw.isOpen()) { throw std::runtime_error("Cannot change parameters while file is open"); }

        // Validate channel count
        if (channels < 1) { throw std::runtime_error("Channel count must be greater or equal to 1"); }
        _channels = channels;
    }

    void Writer::setSamplerate(uint64_t samplerate) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Do not allow settings to change while open
        if (rw.isOpen()) { throw std::runtime_error("Cannot change parameters while file is open"); }

        // Validate samplerate
        if (!samplerate) { throw std::runtime_error("Samplerate must be non-zero"); }
    }

    void Writer::setFormat(Format format) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Do not allow settings to change while open
        if (rw.isOpen()) { throw std::runtime_error("Cannot change parameters while file is open"); }
        _format = format;
    }

    void Writer::setSampleType(SampleType type) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Do not allow settings to change while open
        if (rw.isOpen()) { throw std::runtime_error("Cannot change parameters while file is open"); }
        _type = type;
    }

    void Writer::write(float* samples, int count) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        if (!rw.isOpen()) { return; }
        
        // Select different writer function depending on the chose depth
        int tcount = count * _channels;
        int tbytes = count * bytesPerSamp;
        switch (_type) {
        case SAMP_TYPE_UINT8:
            // Volk doesn't support unsigned ints yet :/
            for (int i = 0; i < tcount; i++) {
                bufU8[i] = (samples[i] * 127.0f) + 128.0f;
            }
            rw.write(bufU8, tbytes);
            break;
        case SAMP_TYPE_INT16:
            volk_32f_s32f_convert_16i(bufI16, samples, 32767.0f, tcount);
            rw.write((uint8_t*)bufI16, tbytes);
            break;
        case SAMP_TYPE_INT32:
            volk_32f_s32f_convert_32i(bufI32, samples, 2147483647.0f, tcount);
            rw.write((uint8_t*)bufI32, tbytes);
            break;
        case SAMP_TYPE_FLOAT32:
            rw.write((uint8_t*)samples, tbytes);
            break;
        default:
            break;
        }

        // Increment sample counter
        samplesWritten += count;
    }
}