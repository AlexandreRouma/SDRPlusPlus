#pragma once

namespace dsp {
    inline uint64_t readBits(int offset, int length, uint8_t* buffer) {
        uint64_t outputValue = 0;

        int lastBit = offset + (length - 1);

        int firstWord = offset / 8;
        int lastWord = lastBit / 8;
        int firstOffset = offset - (firstWord * 8);
        int lastOffset = lastBit - (lastWord * 8);

        int wordCount = (lastWord - firstWord) + 1;

        // If the data fits in a single byte, just get it
        if (wordCount == 1) {
            return (buffer[firstWord] & (0xFF >> firstOffset)) >> (7 - lastOffset);
        }

        int bitsRead = length;
        for (int i = 0; i < wordCount; i++) {
            // First word
            if (i == 0) {
                bitsRead -= 8 - firstOffset;
                outputValue |= (uint64_t)(buffer[firstWord] & (0xFF >> firstOffset)) << bitsRead;
                continue;
            }

            // Last word
            if (i == (wordCount - 1)) {
                outputValue |= (uint64_t)buffer[lastWord] >> (7 - lastOffset);
                break;
            }

            // Just a normal byte
            bitsRead -= 8;
            outputValue |= (uint64_t)buffer[firstWord + i] << bitsRead;
        }

        return outputValue;
    }
}
