#include "conv_codec.h"

namespace ryfi {
    ConvEncoder::ConvEncoder(dsp::stream<uint8_t>* in) {
        // Create the convolutional encoder instance
        conv = correct_convolutional_create(2, 7, correct_conv_r12_7_polynomial);
        
        // Init the base class
        base_type::init(in);
    }

    ConvEncoder::~ConvEncoder() {
        // Destroy the convolutional encoder instance
        correct_convolutional_destroy(conv);
    }

    int ConvEncoder::encode(const uint8_t* in, uint8_t* out, int count) {
        // Run convolutional encoder on the data
        return correct_convolutional_encode(conv, in, count, out);
    }

    int ConvEncoder::run() {
        int count = base_type::_in->read();
        if (count < 0) { return -1; }

        count = encode(base_type::_in->readBuf, base_type::out.writeBuf, count);

        base_type::_in->flush();
        if (!out.swap(count)) { return -1; }
        return count;
    }

    ConvDecoder::ConvDecoder(dsp::stream<dsp::complex_t>* in) {
        // Create the convolutional encoder instance
        conv = correct_convolutional_create(2, 7, correct_conv_r12_7_polynomial);

        // Allocate the soft symbol buffer
        soft = dsp::buffer::alloc<uint8_t>(STREAM_BUFFER_SIZE);
        
        // Init the base class
        base_type::init(in);
    }

    ConvDecoder::~ConvDecoder() {
        // Destroy the convolutional encoder instance
        correct_convolutional_destroy(conv);

        // Free the soft symbol buffer
        dsp::buffer::free(soft);
    }

    int ConvDecoder::decode(const dsp::complex_t* in, uint8_t* out, int count) {
        // Convert to uint8
        const float* _in = (const float*)in;
        count *= 2;
        for (int i = 0; i < count; i++) {
            soft[i] = std::clamp<int>((_in[i] * 127.0f) + 128.0f, 0, 255);
        }
        
        // Run convolutional decoder on the data
        return correct_convolutional_decode_soft(conv, soft, count, out);
    }

    int ConvDecoder::run() {
        int count = base_type::_in->read();
        if (count < 0) { return -1; }

        count = decode(base_type::_in->readBuf, base_type::out.writeBuf, count);

        base_type::_in->flush();
        if (!out.swap(count)) { return -1; }
        return count;
    }
}