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
            // Send out message
            onMessage(addr, msgType, msg);

            // Reset state
            msg.clear();
            currChar = 0;
            currOffset = 0;
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
                    msg += NUMERIC_CHARSET[(data >> 16) & 0b1111];
                    msg += NUMERIC_CHARSET[(data >> 12) & 0b1111];
                    msg += NUMERIC_CHARSET[(data >> 8) & 0b1111];
                    msg += NUMERIC_CHARSET[(data >> 4) & 0b1111];
                    msg += NUMERIC_CHARSET[data & 0b1111];
                }
                else if (msgType == MESSAGE_TYPE_ALPHANUMERIC) {
                    // Unpack ascii bits 7 at a time (TODO: could be more efficient)
                    for (int i = 19; i >= 0; i--) {
                        // Append bit to char
                        currChar |= ((data >> i) & 1) << (currOffset++);

                        // When the char is full, append to message
                        if (currOffset >= 7) {
                            // TODO: maybe replace with std::isprint
                            if (currChar) { msg += currChar; }
                            currChar = 0;
                            currOffset = 0;
                        }
                    }
                }
            }
        }
    }
}