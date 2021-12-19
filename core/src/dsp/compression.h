#pragma once
#include <dsp/block.h>

namespace dsp {
    class DynamicRangeCompressor : public generic_block<DynamicRangeCompressor> {
    public:
        DynamicRangeCompressor() {}

        enum PCMType {
            PCM_TYPE_I8,
            PCM_TYPE_I16,
            PCM_TYPE_F32
        };

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

            float* scaler = (float*)out.writeBuf;
            void* dataBuf = &out.writeBuf[4];

            // If no dynamic range compression is to be done, just pass the data to the output with a null scaler
            if (_pcmType == PCM_TYPE_F32) {
                *scaler = 0;
                memcpy(dataBuf, _in->readBuf, count * sizeof(complex_t));
                _in->flush();
                if (!out.swap(4 + (count * sizeof(complex_t)))) { return -1; }
                return count;
            }

            // Find maximum value
            complex_t val;
            float absre;
            float absim;
            float maxVal = 0;
            for (int i = 0; i < count; i++) {
                val = _in->readBuf[i];
                absre = fabsf(val.re);
                absim = fabsf(val.im);
                if (absre > maxVal) { maxVal = absre; }
                if (absim > maxVal) { maxVal = absim; }
            }

            // Convert to the right type and send it out (sign bit determins pcm type)
            if (_pcmType == PCM_TYPE_I8) {
                *scaler = maxVal;
                volk_32f_s32f_convert_8i((int8_t*)dataBuf, (float*)_in->readBuf, 128.0f / maxVal, count * 2);
                _in->flush();
                if (!out.swap(4 + (count * sizeof(int8_t) * 2))) { return -1; }
            }
            else if (_pcmType == PCM_TYPE_I16) {
                *scaler = -maxVal;
                volk_32f_s32f_convert_16i((int16_t*)dataBuf, (float*)_in->readBuf, 32768.0f / maxVal, count * 2);
                _in->flush();
                if (!out.swap(4 + (count * sizeof(int16_t) * 2))) { return -1; }
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

            float* scaler = (float*)_in->readBuf;
            void* dataBuf = &_in->readBuf[4];

            // If the scaler is null, data is F32
            if (*scaler == 0) {
                memcpy(out.writeBuf, dataBuf, count - 4);
                _in->flush();
                if (!out.swap((count - 4) / sizeof(complex_t))) { return -1; }
                return count;
            }

            // Convert back to f32 from the pcm type
            float absScale = fabsf(*scaler);
            if (*scaler > 0) {
                spdlog::warn("{0}", absScale);
                int outCount = (count - 4) / (sizeof(int8_t) * 2);
                volk_8i_s32f_convert_32f((float*)out.writeBuf, (int8_t*)dataBuf, 128.0f / absScale, outCount * 2);
                _in->flush();
                if (!out.swap(outCount)) { return -1; }
            }
            else {
                int outCount = (count - 4) / (sizeof(int16_t) * 2);
                volk_16i_s32f_convert_32f((float*)out.writeBuf, (int16_t*)dataBuf, 32768.0f / absScale, outCount * 2);
                _in->flush();
                if (!out.swap(outCount)) { return -1; }
            }

            return count;
        }

        stream<complex_t> out;

    private:
        stream<uint8_t>* _in;
    };
}