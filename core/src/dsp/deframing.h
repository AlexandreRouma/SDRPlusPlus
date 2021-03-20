#pragma once
#include <dsp/block.h>
#include <inttypes.h>

#define DSP_SIGN(n)     ((n) >= 0)
#define DSP_STEP(n)     (((n) > 0.0f) ? 1.0f : -1.0f)

namespace dsp {
    class Deframer : public generic_block<Deframer> {
    public:
        Deframer() {}

        Deframer(stream<uint8_t>* in, int frameLen, uint8_t* syncWord, int syncLen) { init(in, frameLen, syncWord, syncLen); }

        ~Deframer() {
            generic_block<Deframer>::stop();
        }

        void init(stream<uint8_t>* in, int frameLen, uint8_t* syncWord, int syncLen) {
            _in = in;
            _frameLen = frameLen;
            _syncword = new  uint8_t[syncLen];
            _syncLen = syncLen;
            memcpy(_syncword, syncWord, syncLen);

            buffer = new uint8_t[STREAM_BUFFER_SIZE + syncLen];
            memset(buffer, 0, syncLen);
            bufferStart = buffer + syncLen;
            
            generic_block<Deframer>::registerInput(_in);
            generic_block<Deframer>::registerOutput(&out);
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
                        nextBitIsStartOfFrame = true;
                    }

                    continue;
                }

                // Else, check for a header
                else if (memcmp(buffer + i, _syncword, _syncLen) == 0) {
                    bitsRead = 0;
                    badFrameCount = 0;
                    continue;
                }
                else if (nextBitIsStartOfFrame) {
                    nextBitIsStartOfFrame = false;

                    // try to save 
                    if (badFrameCount < 5) {
                        badFrameCount++;
                        bitsRead = 0;
                        continue;
                    }

                }

                else { i++; }

                nextBitIsStartOfFrame = false;

            }

            // Keep last _syncLen4 symbols
            memcpy(buffer, &_in->readBuf[count - _syncLen], _syncLen);
            
            //printf("Block processed\n");
            callcount++;

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

        int badFrameCount = 5;
        bool nextBitIsStartOfFrame = false;

        int callcount = 0;
        
        stream<uint8_t>* _in;

    };
}