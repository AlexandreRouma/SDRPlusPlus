#pragma once
#include <dsp/block.h>

namespace dsp {
    enum PCMType {
        PCM_TYPE_I8,
        PCM_TYPE_I16,
        PCM_TYPE_F32
    };

    class DynamicRangeCompressor : public generic_block<DynamicRangeCompressor> {
    public:
        DynamicRangeCompressor() {}

        DynamicRangeCompressor(stream<complex_t>* in, PCMType pcmType) { init(in, pcmType); }

        void init(stream<complex_t>* in, PCMType pcmType) {
            _in = in;
            _pcmType = pcmType;
            generic_block<DynamicRangeCompressor>::registerInput(_in);
            generic_block<DynamicRangeCompressor>::registerOutput(&out);
            generic_block<DynamicRangeCompressor>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<DynamicRangeCompressor>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<DynamicRangeCompressor>::ctrlMtx);
            generic_block<DynamicRangeCompressor>::tempStop();
            generic_block<DynamicRangeCompressor>::unregisterInput(_in);
            _in = in;
            generic_block<DynamicRangeCompressor>::registerInput(_in);
            generic_block<DynamicRangeCompressor>::tempStart();
        }

        void setPCMType(PCMType pcmType) {
            assert(generic_block<DynamicRangeCompressor>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<DynamicRangeCompressor>::ctrlMtx);
            _pcmType = pcmType;
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }
            PCMType type = _pcmType;

            uint16_t* compressionType = (uint16_t*)out.writeBuf;
            uint16_t* sampleType = (uint16_t*)&out.writeBuf[2];
            float* scaler = (float*)&out.writeBuf[4];
            void* dataBuf = &out.writeBuf[8];

            // Write options and leave blank space for compression
            *compressionType = 0;
            *sampleType = type;

            // If type is float32, no compression is needed
            if (type == PCM_TYPE_F32) {
                *scaler = 0;
                memcpy(dataBuf, _in->readBuf, count * sizeof(complex_t));
                _in->flush();
                if (!out.swap(8 + (count * sizeof(complex_t)))) { return -1; }
                return count;
            }

            // Find maximum value
            uint32_t maxIdx;
            volk_32f_index_max_32u(&maxIdx, (float*)_in->readBuf, count * 2);
            float maxVal = ((float*)_in->readBuf)[maxIdx];
            *scaler = maxVal;

            // Convert to the right type and send it out (sign bit determins pcm type)
            if (type == PCM_TYPE_I8) {
                volk_32f_s32f_convert_8i((int8_t*)dataBuf, (float*)_in->readBuf, 128.0f / maxVal, count * 2);
                _in->flush();
                if (!out.swap(8 + (count * sizeof(int8_t) * 2))) { return -1; }
            }
            else if (type == PCM_TYPE_I16) {
                volk_32f_s32f_convert_16i((int16_t*)dataBuf, (float*)_in->readBuf, 32768.0f / maxVal, count * 2);
                _in->flush();
                if (!out.swap(8 + (count * sizeof(int16_t) * 2))) { return -1; }
            }
            else {
                _in->flush();
            }

            return count;
        }

        stream<uint8_t> out;

    private:
        stream<complex_t>* _in;
        PCMType _pcmType;
    };

    class DynamicRangeDecompressor : public generic_block<DynamicRangeDecompressor> {
    public:
        DynamicRangeDecompressor() {}

        DynamicRangeDecompressor(stream<uint8_t>* in) { init(in); }

        void init(stream<uint8_t>* in) {
            _in = in;
            generic_block<DynamicRangeDecompressor>::registerInput(_in);
            generic_block<DynamicRangeDecompressor>::registerOutput(&out);
            generic_block<DynamicRangeDecompressor>::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(generic_block<DynamicRangeDecompressor>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<DynamicRangeDecompressor>::ctrlMtx);
            generic_block<DynamicRangeDecompressor>::tempStop();
            generic_block<DynamicRangeDecompressor>::unregisterInput(_in);
            _in = in;
            generic_block<DynamicRangeDecompressor>::registerInput(_in);
            generic_block<DynamicRangeDecompressor>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            uint16_t sampleType = *(uint16_t*)&_in->readBuf[2];
            float scaler = *(float*)&_in->readBuf[4];
            void* dataBuf = &_in->readBuf[8];

            if (sampleType == PCM_TYPE_F32) {
                memcpy(out.writeBuf, dataBuf, count - 8);
                _in->flush();
                if (!out.swap((count - 8) / sizeof(complex_t))) { return -1; }
            }
            else if (sampleType == PCM_TYPE_I16) {
                int outCount = (count - 8) / (sizeof(int16_t) * 2);
                volk_16i_s32f_convert_32f((float*)out.writeBuf, (int16_t*)dataBuf, 32768.0f / scaler, outCount * 2);
                _in->flush();
                if (!out.swap(outCount)) { return -1; }
            }
            else if (sampleType == PCM_TYPE_I8) {
                int outCount = (count - 8) / (sizeof(int8_t) * 2);
                volk_8i_s32f_convert_32f((float*)out.writeBuf, (int8_t*)dataBuf, 128.0f / scaler, outCount * 2);
                _in->flush();
                if (!out.swap(outCount)) { return -1; }
            }
            else {
                _in->flush();
            }

            return count;
        }

        stream<complex_t> out;

    private:
        stream<uint8_t>* _in;
    };
}