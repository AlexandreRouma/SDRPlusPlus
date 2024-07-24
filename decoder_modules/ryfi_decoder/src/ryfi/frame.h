#pragma once
#include <stdint.h>
#include "rs_codec.h"

namespace ryfi {
    enum PacketOffset {
        PKT_OFFS_NONE   = 0xFFFF
    };
    
    struct Frame {
        /**
         * Serialize the frame to bytes.
         * @param bytes Buffer to write the serialized frame to.
        */
        int serialize(uint8_t* bytes) const;

        /**
         * Deserialize a frame from bytes.
         * @param bytes Buffer to deserialize the frame from.
         * @param frame Object that will contain the deserialize frame.
        */
        static void deserialize(const uint8_t* bytes, Frame& frame);

        // Size of a serialized frame
        static inline const int FRAME_SIZE      = RS_BLOCK_DEC_SIZE*RS_BLOCK_COUNT;

        // Size of the data area of the frame
        static inline const int FRAME_DATA_SIZE = FRAME_SIZE - 6;

        // Steadily increasing counter.
        uint16_t counter = 0;

        // Byte offset of the first packet in the frame.
        uint16_t firstPacket = 0;

        // Byte offset of the last packet in the frame.
        uint16_t lastPacket = 0;

        // Data area of the frame.
        uint8_t content[FRAME_DATA_SIZE];
    };
}