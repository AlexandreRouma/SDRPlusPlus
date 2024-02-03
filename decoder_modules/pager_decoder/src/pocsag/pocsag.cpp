#include "pocsag.h"
#include <string.h>
#include <utils/flog.h>

#define POCSAG_FRAME_SYNC_CODEWORD  ((uint32_t)(0b01111100110100100001010111011000))
#define POCSAG_IDLE_CODEWORD_DATA   ((uint32_t)(0b011110101100100111000))
#define POCSAG_BATCH_BIT_COUNT      (POCSAG_BATCH_CODEWORD_COUNT*32)
#define POCSAG_DATA_BITS_PER_CW     20

#define POCSAG_GEN_POLY             ((uint32_t)(0b11101101001))

namespace pocsag {
    const char NUMERIC_CHARSET[] = {
        '0',
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        '*',
        'U',
        ' ',
        '-',
        ']',
        '['
    };

    Decoder::Decoder() {
        // Zero out batch
        memset(batch, 0, sizeof(batch));
    }

    void Decoder::process(uint8_t* symbols, int count) {
        for (int i = 0; i < count; i++) {
            // Get symbol
            uint32_t s = symbols[i];

            // If not sync, try to acquire sync (TODO: sync confidence)
            if (!synced) {
                // Append new symbol to sync shift register
                syncSR = (syncSR << 1) | s;

                // Test for sync
                synced = (distance(syncSR, POCSAG_FRAME_SYNC_CODEWORD) <= POCSAG_SYNC_DIST);

                // Go to next symbol
                continue;
            }

            // TODO: Flush message on desync

            // Append bit to batch
            batch[batchOffset >> 5] |= (s << (31 - (batchOffset & 0b11111)));
            batchOffset++;

            // On end of batch, decode and reset
            if (batchOffset >= POCSAG_BATCH_BIT_COUNT) {
                decodeBatch();
                batchOffset = 0;
                synced = false;
                memset(batch, 0, sizeof(batch));
            }
        }
    }

    int Decoder::distance(uint32_t a, uint32_t b) {
        uint32_t diff = a ^ b;
        int dist = 0;
        for (int i = 0; i < 32; i++) {
            dist += (diff >> i ) & 1;
        }
        return dist;
    }

    bool Decoder::correctCodeword(Codeword in, Codeword& out) {


        return true; // TODO
    }

    void Decoder::flushMessage() {
        if (!msg.empty()) {

            // Unpack bits
            std::string outStr = "";
            for (int i = 0; (i+7) <= msg.size(); i += 7) {
                uint8_t b0 = msg[i];
                uint8_t b1 = msg[i+1];
                uint8_t b2 = msg[i+2];
                uint8_t b3 = msg[i+3];
                uint8_t b4 = msg[i+4];
                uint8_t b5 = msg[i+5];
                uint8_t b6 = msg[i+6];
                outStr += (char)((b6<<6) | (b5<<5) | (b4<<4) | (b3<<3) | (b2<<2) | (b1<<1) | b0);
            }
            onMessage(addr, msgType, outStr);

            // // Send out message
            // onMessage(addr, msgType, msg);

            // Reset state
            msg.clear();
            leftInLast = 0;
        }
    }

    void printbin(uint32_t cw) {
        for (int i = 31; i >= 0; i--) {
            printf("%c", ((cw >> i) & 1) ? '1':'0');
        }
    }

    void bitswapChar(char in, char& out) {
        out = 0;
        for (int i = 0; i < 7; i++) {
            out |= ((in >> (6-i)) & 1) << i;
        }
    }

    void Decoder::decodeBatch() {
        for (int i = 0; i < POCSAG_BATCH_CODEWORD_COUNT; i++) {
            // Get codeword
            Codeword cw = batch[i];

            // Correct errors. If corrupted, skip
            if (!correctCodeword(cw, cw)) { continue; }
            // TODO: End message if two consecutive are corrupt

            // Get codeword type
            CodewordType type = (CodewordType)((cw >> 31) & 1);
            if (type == CODEWORD_TYPE_ADDRESS && (cw >> 11) == POCSAG_IDLE_CODEWORD_DATA) {
                type = CODEWORD_TYPE_IDLE;
            }

            // Decode codeword
            if (type == CODEWORD_TYPE_IDLE) {
                // If a non-empty message is available, send it out and clear
                flushMessage();
            }
            else if (type == CODEWORD_TYPE_ADDRESS) {
                // If a non-empty message is available, send it out and clear
                flushMessage();

                // Decode message type
                msgType = MESSAGE_TYPE_ALPHANUMERIC;
                // msgType = (MessageType)((cw >> 11) & 0b11);

                // Decode address and append lower 8 bits from position
                addr = ((cw >> 13) & 0b111111111111111111) << 3;
                addr |= (i >> 1);
            }
            else if (type == CODEWORD_TYPE_MESSAGE) {
                // Extract the 20 data bits
                uint32_t data = (cw >> 11) & 0b11111111111111111111;

                // Decode data depending on message type
                if (msgType == MESSAGE_TYPE_NUMERIC) {
                    // Numeric messages pack 5 characters per message codeword
                    //msg += NUMERIC_CHARSET[(data >> 16) & 0b1111];
                    //msg += NUMERIC_CHARSET[(data >> 12) & 0b1111];
                    //msg += NUMERIC_CHARSET[(data >> 8) & 0b1111];
                    //msg += NUMERIC_CHARSET[(data >> 4) & 0b1111];
                    //msg += NUMERIC_CHARSET[data & 0b1111];
                }
                else if (msgType == MESSAGE_TYPE_ALPHANUMERIC) {
                    // // Alpha messages pack 7bit characters in the entire codeword stream
                    // int lasti;
                    // for (int i = -leftInLast; i <= POCSAG_DATA_BITS_PER_CW-7; i += 7) {
                    //     // Read 7 bits
                    //     char c = 0;
                    //     if (i < 0) {
                    //         c = (lastMsgData & ((1 << (-i)) - 1)) << (7+i);
                    //     }
                    //     c |= (data >> (13 - i)) & 0b1111111;

                    //     // Save character
                    //     bitswapChar(c, c);
                    //     msg += c;

                    //     // Update last successful unpack
                    //     lasti = i;
                    // }

                    // // Save how much is still left to read
                    // leftInLast = 20 - (lasti + 7);

                    // Pack the bits backwards
                    for (int i = 19; i >= 0; i--) {
                        msg += (char)((data >> i) & 1);
                    }
                }

                // Save last data
                lastMsgData = data;
            }
        }
    }
}