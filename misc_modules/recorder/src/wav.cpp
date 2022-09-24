#include "wav.h"
#include <volk/volk.h>
#include <stdexcept>
#include <dsp/buffer/buffer.h>
#include <dsp/stream.h>

namespace wav {
    const char* RIFF_SIGNATURE          = "RIFF";
    const char* WAVE_FILE_TYPE          = "WAVE";
    const char* FORMAT_MARKER           = "fmt ";
    const char* DATA_MARKER             = "data";
    const uint32_t FORMAT_HEADER_LEN    = 16;
    const uint16_t SAMPLE_TYPE_PCM      = 1;
    
    Writer::Writer(int channels, uint64_t samplerate, Format format, SampleDepth depth) {
        // Validate channels and samplerate
        if (channels < 1) { throw std::runtime_error("Channel count must be greater or equal to 1"); }
        if (!samplerate) { throw std::runtime_error("Samplerate must be non-zero"); }
        
        // Initialize variables
        _channels = channels;
        _samplerate = samplerate;
        _format = format;
        _depth = depth;

        // Initialize header with constants
        memcpy(hdr.signature, RIFF_SIGNATURE, 4);
        memcpy(hdr.fileType, WAVE_FILE_TYPE, 4);
        memcpy(hdr.formatMarker, FORMAT_MARKER, 4);
        hdr.formatHeaderLength = FORMAT_HEADER_LEN;
        hdr.sampleType = SAMPLE_TYPE_PCM;
        memcpy(hdr.dataMarker, DATA_MARKER, 4);
    }

    Writer::~Writer() { close(); }

    bool Writer::open(std::string path) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Close previous file
        if (_isOpen) { close(); }

        // Reset work values
        samplesWritten = 0;

        // Precompute sizes and allocate buffers
        bytesPerSamp = (_depth / 8) * _channels;
        switch (_depth) {
        case SAMP_DEPTH_8BIT:
            buf8 = dsp::buffer::alloc<int8_t>(STREAM_BUFFER_SIZE * _channels);
            break;
        case SAMP_DEPTH_16BIT:
            buf16 = dsp::buffer::alloc<int16_t>(STREAM_BUFFER_SIZE * _channels);
            break;
        case SAMP_DEPTH_32BIT:
            buf32 = dsp::buffer::alloc<int32_t>(STREAM_BUFFER_SIZE * _channels);
            break;
        default:
            return false;
            break;
        }

        // Open new file
        file.open(path, std::ios::out | std::ios::binary);
        if (!file.is_open()) { return false; }

        // Skip header, it'll be written when finalizing the file
        uint8_t dummy[sizeof(Header)];
        memset(dummy, 0, sizeof(dummy));
        file.write((char*)dummy, sizeof(dummy));

        _isOpen = true;
        return true;
    }

    bool Writer::isOpen() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        return _isOpen;
    }

    void Writer::close() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Do nothing if the file is not open
        if (!_isOpen) { return; }

        // Finilize wav
        finalize();

        // Close the file
        file.close();

        // Destroy buffers
        switch (_depth) {
        case SAMP_DEPTH_8BIT:
            dsp::buffer::free(buf8);
            break;
        case SAMP_DEPTH_16BIT:
            dsp::buffer::free(buf16);
            break;
        case SAMP_DEPTH_32BIT:
            dsp::buffer::free(buf32);
            break;
        default:
            break;
        }

        // Mark as closed
        _isOpen = false;
    }

    void Writer::setChannels(int channels) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Do not allow settings to change while open
        if (_isOpen) { throw std::runtime_error("Cannot change parameters while file is open"); }

        // Validate channel count
        if (channels < 1) { throw std::runtime_error("Channel count must be greater or equal to 1"); }
        _channels = channels;
    }

    void Writer::setSamplerate(uint64_t samplerate) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Do not allow settings to change while open
        if (_isOpen) { throw std::runtime_error("Cannot change parameters while file is open"); }

        // Validate samplerate
        if (!samplerate) { throw std::runtime_error("Samplerate must be non-zero"); }
    }

    void Writer::setFormat(Format format) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Do not allow settings to change while open
        if (_isOpen) { throw std::runtime_error("Cannot change parameters while file is open"); }
        _format = format;
    }

    void Writer::setSampleDepth(SampleDepth depth) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        // Do not allow settings to change while open
        if (_isOpen) { throw std::runtime_error("Cannot change parameters while file is open"); }
        _depth = depth;
    }

    void Writer::write(float* samples, int count) {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        if (!_isOpen) { return; }
        
        // Select different writer function depending on the chose depth
        int tcount;
        switch (_depth) {
        case SAMP_DEPTH_8BIT:
            tcount = count * _channels;
            // Volk doesn't support unsigned ints :/
            for (int i = 0; i < tcount; i++) {
                buf8[i] = (samples[i] * 127.0f) + 128.0f;
            }
            file.write((char*)buf8, count * bytesPerSamp);
            break;
        case SAMP_DEPTH_16BIT:
            volk_32f_s32f_convert_16i(buf16, samples, 32767.0f, count * _channels);
            file.write((char*)buf16, count * bytesPerSamp);
            break;
        case SAMP_DEPTH_32BIT:
            volk_32f_s32f_convert_32i(buf32, samples, 2147483647.0f, count * _channels);
            file.write((char*)buf32, count * bytesPerSamp);
            break;
        default:
            break;
        }

        // Increment sample counter
        samplesWritten += count;
    }

    void Writer::finalize() {
        file.seekp(file.beg);
        hdr.channelCount = _channels;
        hdr.sampleRate = _samplerate;
        hdr.bitDepth = _depth;
        hdr.bytesPerSample = bytesPerSamp;
        hdr.bytesPerSecond = bytesPerSamp * _samplerate;
        hdr.dataSize = samplesWritten * bytesPerSamp;
        hdr.fileSize = hdr.dataSize + sizeof(hdr) - 8;
        file.write((char*)&hdr, sizeof(hdr));
    }
}