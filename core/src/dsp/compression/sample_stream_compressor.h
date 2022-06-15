#pragma once
#include "../processor.h"
#include "pcm_type.h"

namespace dsp::compression {
    class SampleStreamCompressor : public Processor<complex_t, uint8_t> {
        using base_type = Processor<complex_t, uint8_t>;
    public:
        SampleStreamCompressor() {}

        SampleStreamCompressor(stream<complex_t>* in, PCMType pcmType) { init(in, pcmType); }

        void init(stream<complex_t>* in, PCMType pcmType) {
            _pcmType = pcmType;
            base_type::init(in);
        }

        void setPCMType(PCMType pcmType) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _pcmType = pcmType;
            base_type::tempStart();
        }

        inline static int process(int count, PCMType pcmType, const complex_t* in, uint8_t* out) {
            uint16_t* compressionType = (uint16_t*)out;
            uint16_t* sampleType = (uint16_t*)&out[2];
            float* scaler = (float*)&out[4];
            void* dataBuf = &out[8];

            // Write options and leave blank space for compression
            *compressionType = 0;
            *sampleType = pcmType;

            // If type is float32, no compression is needed
            if (pcmType == PCMType::PCM_TYPE_F32) {
                *scaler = 0;
                memcpy(dataBuf, in, count * sizeof(complex_t));
                return count;
            }

            // Find maximum value
            uint32_t maxIdx;
            volk_32f_index_max_32u(&maxIdx, (float*)in, count * 2);
            float maxVal = ((float*)in)[maxIdx];
            *scaler = maxVal;

            // Convert to the right type and send it out (sign bit determines pcm type)
            if (pcmType == PCMType::PCM_TYPE_I8) {
                volk_32f_s32f_convert_8i((int8_t*)dataBuf, (float*)in, 128.0f / maxVal, count * 2);
                return 8 + (count * sizeof(int8_t) * 2);
            }
            else if (pcmType == PCMType::PCM_TYPE_I16) {
                volk_32f_s32f_convert_16i((int16_t*)dataBuf, (float*)in, 32768.0f / maxVal, count * 2);
                return 8 + (count * sizeof(int16_t) * 2);
            }

            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, _pcmType, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    protected:
        PCMType _pcmType;
    };
}