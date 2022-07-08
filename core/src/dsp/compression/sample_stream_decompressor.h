#pragma once
#include "../processor.h"
#include "pcm_type.h"

namespace dsp::compression {
    class SampleStreamDecompressor : public Processor<uint8_t, complex_t> {
        using base_type = Processor<uint8_t, complex_t>;
    public:
        SampleStreamDecompressor() {}

        SampleStreamDecompressor(stream<uint8_t>* in) { base_type::init(in); }

        inline int process(int count, const uint8_t* in, complex_t* out) {
            uint16_t sampleType = *(uint16_t*)&in[2];
            float scaler = *(float*)&in[4];
            const void* dataBuf = &in[8];

            if (sampleType == PCMType::PCM_TYPE_F32) {
                memcpy(out, dataBuf, count - 8);
                return (count - 8) / sizeof(complex_t);
            }
            else if (sampleType == PCMType::PCM_TYPE_I16) {
                int outCount = (count - 8) / (sizeof(int16_t) * 2);
                volk_16i_s32f_convert_32f((float*)out, (int16_t*)dataBuf, 32768.0f / scaler, outCount * 2);
                return outCount;
            }
            else if (sampleType == PCMType::PCM_TYPE_I8) {
                int outCount = (count - 8) / (sizeof(int8_t) * 2);
                volk_8i_s32f_convert_32f((float*)out, (int8_t*)dataBuf, 128.0f / scaler, outCount * 2);
                return outCount;
            }
            
            return 0;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }
    };
}