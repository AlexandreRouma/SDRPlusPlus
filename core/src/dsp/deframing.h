#pragma once
#include <dsp/block.h>
#include <inttypes.h>

#define DSP_SIGN(n) ((n) >= 0)
#define DSP_STEP(n) (((n) > 0.0f) ? 1.0f : -1.0f)

namespace dsp {
    class Deframer : public generic_block<Deframer> {
    public:
        Deframer() {}

        Deframer(stream<uint8_t>* in, int frameLen, uint8_t* syncWord, int syncLen) { init(in, frameLen, syncWord, syncLen); }

        ~Deframer() {
            if (!generic_block<Deframer>::_block_init) { return; }
            generic_block<Deframer>::stop();
            generic_block<Deframer>::_block_init = false;
        }

        void init(stream<uint8_t>* in, int frameLen, uint8_t* syncWord, int syncLen) {
            _in = in;
            _frameLen = frameLen;
            _syncword = new uint8_t[syncLen];
            _syncLen = syncLen;
            memcpy(_syncword, syncWord, syncLen);

            buffer = new uint8_t[STREAM_BUFFER_SIZE + syncLen];
            memset(buffer, 0, syncLen);
            bufferStart = buffer + syncLen;

            generic_block<Deframer>::registerInput(_in);
            generic_block<Deframer>::registerOutput(&out);
            generic_block<Deframer>::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(generic_block<Deframer>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<Deframer>::ctrlMtx);
            generic_block<Deframer>::tempStop();
            generic_block<Deframer>::unregisterInput(_in);
            _in = in;
            generic_block<Deframer>::registerInput(_in);
            generic_block<Deframer>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            // Copy data into work buffer
            memcpy(bufferStart, _in->readBuf, count - 1);

            // Iterate through all symbols
            for (int i = 0; i < count;) {

                // If already in the process of reading bits
                if (bitsRead >= 0) {
                    if ((bitsRead % 8) == 0) { out.writeBuf[bitsRead / 8] = 0; }
                    out.writeBuf[bitsRead / 8] |= (buffer[i] << (7 - (bitsRead % 8)));
                    i++;
                    bitsRead++;

                    if (bitsRead >= _frameLen) {
                        if (!out.swap((bitsRead / 8) + ((bitsRead % 8) > 0))) { return -1; }
                        bitsRead = -1;
                        if (allowSequential) { nextBitIsStartOfFrame = true; }
                    }

                    continue;
                }

                // Else, check for a header
                else if (memcmp(buffer + i, _syncword, _syncLen) == 0) {
                    bitsRead = 0;
                    //printf("Frame found!\n");
                    badFrameCount = 0;
                    continue;
                }
                else if (nextBitIsStartOfFrame) {
                    nextBitIsStartOfFrame = false;

                    // try to save
                    if (badFrameCount < 5) {
                        badFrameCount++;
                        //printf("Frame found!\n");
                        bitsRead = 0;
                        continue;
                    }
                }

                else {
                    i++;
                }

                nextBitIsStartOfFrame = false;
            }

            // Keep last _syncLen4 symbols
            memcpy(buffer, &_in->readBuf[count - _syncLen], _syncLen);

            //printf("Block processed\n");
            callcount++;

            _in->flush();
            return count;
        }

        bool allowSequential = true;

        stream<uint8_t> out;

    private:
        uint8_t* buffer;
        uint8_t* bufferStart;
        uint8_t* _syncword;
        int count;
        int _frameLen;
        int _syncLen;
        int bitsRead = -1;

        int badFrameCount = 5;
        bool nextBitIsStartOfFrame = false;

        int callcount = 0;

        stream<uint8_t>* _in;
    };

    inline int MachesterHammingDistance(float* data, uint8_t* syncBits, int n) {
        int dist = 0;
        for (int i = 0; i < n; i++) {
            if ((data[(2 * i) + 1] > data[2 * i]) != syncBits[i]) { dist++; }
        }
        return dist;
    }

    inline int HammingDistance(uint8_t* data, uint8_t* syncBits, int n) {
        int dist = 0;
        for (int i = 0; i < n; i++) {
            if (data[i] != syncBits[i]) { dist++; }
        }
        return dist;
    }

    class ManchesterDeframer : public generic_block<ManchesterDeframer> {
    public:
        ManchesterDeframer() {}

        ManchesterDeframer(stream<float>* in, int frameLen, uint8_t* syncWord, int syncLen) { init(in, frameLen, syncWord, syncLen); }

        void init(stream<float>* in, int frameLen, uint8_t* syncWord, int syncLen) {
            _in = in;
            _frameLen = frameLen;
            _syncword = new uint8_t[syncLen];
            _syncLen = syncLen;
            memcpy(_syncword, syncWord, syncLen);

            buffer = new float[STREAM_BUFFER_SIZE + (syncLen * 2)];
            memset(buffer, 0, syncLen * 2 * sizeof(float));
            bufferStart = &buffer[syncLen * 2];

            generic_block<ManchesterDeframer>::registerInput(_in);
            generic_block<ManchesterDeframer>::registerOutput(&out);
            generic_block<ManchesterDeframer>::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(generic_block<ManchesterDeframer>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<ManchesterDeframer>::ctrlMtx);
            generic_block<ManchesterDeframer>::tempStop();
            generic_block<ManchesterDeframer>::unregisterInput(_in);
            _in = in;
            generic_block<ManchesterDeframer>::registerInput(_in);
            generic_block<ManchesterDeframer>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            int readable;

            // Copy data into work buffer
            memcpy(bufferStart, _in->readBuf, (count - 1) * sizeof(float));

            // Iterate through all symbols
            for (int i = 0; i < count;) {

                // If already in the process of reading bits
                if (bitsRead >= 0) {
                    readable = std::min<int>(count - i, _frameLen - bitsRead);
                    memcpy(&out.writeBuf[bitsRead], &buffer[i], readable * sizeof(float));
                    bitsRead += readable;
                    i += readable;
                    if (bitsRead >= _frameLen) {
                        out.swap(_frameLen);
                        bitsRead = -1;
                    }
                    continue;
                }

                // Else, check for a header
                if (MachesterHammingDistance(&buffer[i], _syncword, _syncLen) <= 2) {
                    bitsRead = 0;
                    continue;
                }

                i++;
            }

            // Keep last _syncLen symbols
            memcpy(buffer, &_in->readBuf[count - (_syncLen * 2)], _syncLen * 2 * sizeof(float));

            _in->flush();
            return count;
        }

        stream<float> out;

    private:
        float* buffer;
        float* bufferStart;
        uint8_t* _syncword;
        int count;
        int _frameLen;
        int _syncLen;
        int bitsRead = -1;

        stream<float>* _in;
    };

    class SymbolDeframer : public generic_block<SymbolDeframer> {
    public:
        SymbolDeframer() {}

        SymbolDeframer(stream<uint8_t>* in, int frameLen, uint8_t* syncWord, int syncLen) { init(in, frameLen, syncWord, syncLen); }

        void init(stream<uint8_t>* in, int frameLen, uint8_t* syncWord, int syncLen) {
            _in = in;
            _frameLen = frameLen;
            _syncword = new uint8_t[syncLen];
            _syncLen = syncLen;
            memcpy(_syncword, syncWord, syncLen);

            buffer = new uint8_t[STREAM_BUFFER_SIZE + syncLen];
            memset(buffer, 0, syncLen);
            bufferStart = &buffer[syncLen];

            generic_block<SymbolDeframer>::registerInput(_in);
            generic_block<SymbolDeframer>::registerOutput(&out);
            generic_block<SymbolDeframer>::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(generic_block<SymbolDeframer>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<SymbolDeframer>::ctrlMtx);
            generic_block<SymbolDeframer>::tempStop();
            generic_block<SymbolDeframer>::unregisterInput(_in);
            _in = in;
            generic_block<SymbolDeframer>::registerInput(_in);
            generic_block<SymbolDeframer>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            int readable;

            // Copy data into work buffer
            memcpy(bufferStart, _in->readBuf, count - 1);

            // Iterate through all symbols
            for (int i = 0; i < count;) {

                // If already in the process of reading bits
                if (bitsRead >= 0) {
                    readable = std::min<int>(count - i, _frameLen - bitsRead);
                    memcpy(&out.writeBuf[bitsRead], &buffer[i], readable);
                    bitsRead += readable;
                    i += readable;
                    if (bitsRead >= _frameLen) {
                        out.swap(_frameLen);
                        bitsRead = -1;
                    }
                    continue;
                }

                // Else, check for a header
                if (HammingDistance(&buffer[i], _syncword, _syncLen) <= 2) {
                    bitsRead = 0;
                    continue;
                }

                i++;
            }

            // Keep last _syncLen symbols
            memcpy(buffer, &_in->readBuf[count - _syncLen], _syncLen);

            _in->flush();
            return count;
        }

        stream<uint8_t> out;

    private:
        uint8_t* buffer;
        uint8_t* bufferStart;
        uint8_t* _syncword;
        int count;
        int _frameLen;
        int _syncLen;
        int bitsRead = -1;

        stream<uint8_t>* _in;
    };

    class ManchesterDecoder : public generic_block<ManchesterDecoder> {
    public:
        ManchesterDecoder() {}

        ManchesterDecoder(stream<float>* in, bool inverted) { init(in, inverted); }

        void init(stream<float>* in, bool inverted) {
            _in = in;
            _inverted = inverted;
            generic_block<ManchesterDecoder>::registerInput(_in);
            generic_block<ManchesterDecoder>::registerOutput(&out);
            generic_block<ManchesterDecoder>::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(generic_block<ManchesterDecoder>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<ManchesterDecoder>::ctrlMtx);
            generic_block<ManchesterDecoder>::tempStop();
            generic_block<ManchesterDecoder>::unregisterInput(_in);
            _in = in;
            generic_block<ManchesterDecoder>::registerInput(_in);
            generic_block<ManchesterDecoder>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            if (_inverted) {
                for (int i = 0; i < count; i += 2) {
                    out.writeBuf[i / 2] = (_in->readBuf[i + 1] < _in->readBuf[i]);
                }
            }
            else {
                for (int i = 0; i < count; i += 2) {
                    out.writeBuf[i / 2] = (_in->readBuf[i + 1] > _in->readBuf[i]);
                }
            }

            _in->flush();
            out.swap(count / 2);
            return count;
        }

        stream<uint8_t> out;

    private:
        stream<float>* _in;
        bool _inverted;
    };

    class BitPacker : public generic_block<BitPacker> {
    public:
        BitPacker() {}

        BitPacker(stream<uint8_t>* in) { init(in); }

        void init(stream<uint8_t>* in) {
            _in = in;

            generic_block<BitPacker>::registerInput(_in);
            generic_block<BitPacker>::registerOutput(&out);
            generic_block<BitPacker>::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(generic_block<BitPacker>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<BitPacker>::ctrlMtx);
            generic_block<BitPacker>::tempStop();
            generic_block<BitPacker>::unregisterInput(_in);
            _in = in;
            generic_block<BitPacker>::registerInput(_in);
            generic_block<BitPacker>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            for (int i = 0; i < count; i++) {
                if ((i % 8) == 0) { out.writeBuf[i / 8] = 0; }
                out.writeBuf[i / 8] |= (_in->readBuf[i] & 1) << (7 - (i % 8));
            }

            _in->flush();
            out.swap((count / 8) + (((count % 8) == 0) ? 0 : 1));
            return count;
        }

        stream<uint8_t> out;

    private:
        stream<uint8_t>* _in;
    };
}