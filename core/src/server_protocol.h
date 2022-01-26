#pragma once
#include <stdint.h>
#include <gui/smgui.h>
#include <dsp/types.h>

#define SERVER_MAX_PACKET_SIZE  (STREAM_BUFFER_SIZE * sizeof(dsp::complex_t) * 2)

namespace server {
    enum PacketType {
        // Client to Server
        PACKET_TYPE_COMMAND,
        PACKET_TYPE_COMMAND_ACK,
        PACKET_TYPE_BASEBAND,
        PACKET_TYPE_BASEBAND_COMPRESSED,
        PACKET_TYPE_VFO,
        PACKET_TYPE_FFT,
        PACKET_TYPE_ERROR
    };

    enum Command {
        // Client to Server
        COMMAND_GET_UI = 0x00,
        COMMAND_UI_ACTION,
        COMMAND_START,
        COMMAND_STOP,
        COMMAND_SET_FREQUENCY,
        COMMAND_GET_SAMPLERATE,
        COMMAND_SET_SAMPLE_TYPE,
        COMMAND_SET_COMPRESSION,

        // Server to client
        COMMAND_SET_SAMPLERATE = 0x80,
        COMMAND_DISCONNECT
    };

    enum Error {
        ERROR_NONE = 0x00,
        ERROR_INVALID_PACKET,
        ERROR_INVALID_COMMAND,
        ERROR_INVALID_ARGUMENT
    };
    
#pragma pack(push, 1)
    struct PacketHeader {
        uint32_t type;
        uint32_t size;
    };

    struct CommandHeader {
        uint32_t cmd;
    };
#pragma pack(pop)
}