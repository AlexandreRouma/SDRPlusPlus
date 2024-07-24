#include "frame.h"

namespace ryfi {
    int Frame::serialize(uint8_t* bytes) const {
        // Write the counter
        bytes[0] = (counter >> 8) & 0xFF;
        bytes[1] = counter & 0xFF;

        // Write the first packet pointer
        bytes[2] = (firstPacket >> 8) & 0xFF;
        bytes[3] = firstPacket & 0xFF;

        // Write the last packet pointer
        bytes[4] = (lastPacket >> 8) & 0xFF;
        bytes[5] = lastPacket & 0xFF;

        // Write the data
        memcpy(&bytes[6], content, FRAME_DATA_SIZE);

        // Return the length of a serialized frame
        return FRAME_SIZE;
    }

    void Frame::deserialize(const uint8_t* bytes, Frame& frame) {
        // Read the counter
        frame.counter = (((uint16_t)bytes[0]) << 8) | ((uint16_t)bytes[1]);

        // Read the first packet pointer
        frame.firstPacket = (((uint16_t)bytes[2]) << 8) | ((uint16_t)bytes[3]);

        // Read the last packet pointer
        frame.lastPacket = (((uint16_t)bytes[4]) << 8) | ((uint16_t)bytes[5]);

        // Read the data
        memcpy(frame.content, &bytes[6], FRAME_DATA_SIZE);
    }
}