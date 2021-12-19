/*

SPY Server protocol structures and constants
Copyright (C) 2017 Youssef Touil youssef@live.com

Corrections by Ryzerth.

*/


#pragma once

#include <stdint.h>
#include <limits.h>

#define SPYSERVER_PROTOCOL_VERSION (((2) << 24) | ((0) << 16) | (1700))

#define SPYSERVER_MAX_COMMAND_BODY_SIZE (256)
#define SPYSERVER_MAX_MESSAGE_BODY_SIZE (1 << 20)
#define SPYSERVER_MAX_DISPLAY_PIXELS    (1 << 15)
#define SPYSERVER_MIN_DISPLAY_PIXELS    (100)
#define SPYSERVER_MAX_FFT_DB_RANGE      (150)
#define SPYSERVER_MIN_FFT_DB_RANGE      (10)
#define SPYSERVER_MAX_FFT_DB_OFFSET     (100)

enum SpyServerDeviceType {
    SPYSERVER_DEVICE_INVALID = 0,
    SPYSERVER_DEVICE_AIRSPY_ONE = 1,
    SPYSERVER_DEVICE_AIRSPY_HF = 2,
    SPYSERVER_DEVICE_RTLSDR = 3,
};

enum SpyServerCommandType {
    SPYSERVER_CMD_HELLO = 0,
    SPYSERVER_CMD_SET_SETTING = 2,
    SPYSERVER_CMD_PING = 3,
};

enum SpyServerSettingType {
    SPYSERVER_SETTING_STREAMING_MODE = 0,
    SPYSERVER_SETTING_STREAMING_ENABLED = 1,
    SPYSERVER_SETTING_GAIN = 2,

    SPYSERVER_SETTING_IQ_FORMAT = 100,       // 0x64
    SPYSERVER_SETTING_IQ_FREQUENCY = 101,    // 0x65
    SPYSERVER_SETTING_IQ_DECIMATION = 102,   // 0x66
    SPYSERVER_SETTING_IQ_DIGITAL_GAIN = 103, // 0x67

    SPYSERVER_SETTING_FFT_FORMAT = 200,         // 0xc8
    SPYSERVER_SETTING_FFT_FREQUENCY = 201,      // 0xc9
    SPYSERVER_SETTING_FFT_DECIMATION = 202,     // 0xca
    SPYSERVER_SETTING_FFT_DB_OFFSET = 203,      // 0xcb
    SPYSERVER_SETTING_FFT_DB_RANGE = 204,       // 0xcc
    SPYSERVER_SETTING_FFT_DISPLAY_PIXELS = 205, // 0xcd
};

enum SpyServerStreamType {
    SPYSERVER_STREAM_TYPE_STATUS = 0,
    SPYSERVER_STREAM_TYPE_IQ = 1,
    SPYSERVER_STREAM_TYPE_AF = 2,
    SPYSERVER_STREAM_TYPE_FFT = 4,
};

enum SpyServerStreamingMode {
    SPYSERVER_STREAM_MODE_IQ_ONLY = SPYSERVER_STREAM_TYPE_IQ,                            // 0x01
    SPYSERVER_STREAM_MODE_AF_ONLY = SPYSERVER_STREAM_TYPE_AF,                            // 0x02
    SPYSERVER_STREAM_MODE_FFT_ONLY = SPYSERVER_STREAM_TYPE_FFT,                          // 0x04
    SPYSERVER_STREAM_MODE_FFT_IQ = SPYSERVER_STREAM_TYPE_FFT | SPYSERVER_STREAM_TYPE_IQ, // 0x05
    SPYSERVER_STREAM_MODE_FFT_AF = SPYSERVER_STREAM_TYPE_FFT | SPYSERVER_STREAM_TYPE_AF, // 0x06
};

enum SpyServerStreamFormat {
    SPYSERVER_STREAM_FORMAT_INVALID = 0,
    SPYSERVER_STREAM_FORMAT_UINT8 = 1,
    SPYSERVER_STREAM_FORMAT_INT16 = 2,
    SPYSERVER_STREAM_FORMAT_INT24 = 3,
    SPYSERVER_STREAM_FORMAT_FLOAT = 4,
    SPYSERVER_STREAM_FORMAT_DINT4 = 5,
};

enum SpyServerMessageType {
    SPYSERVER_MSG_TYPE_DEVICE_INFO = 0,
    SPYSERVER_MSG_TYPE_CLIENT_SYNC = 1,
    SPYSERVER_MSG_TYPE_PONG = 2,
    SPYSERVER_MSG_TYPE_READ_SETTING = 3,

    SPYSERVER_MSG_TYPE_UINT8_IQ = 100, // 0x64
    SPYSERVER_MSG_TYPE_INT16_IQ = 101, // 0x65
    SPYSERVER_MSG_TYPE_INT24_IQ = 102, // 0x66
    SPYSERVER_MSG_TYPE_FLOAT_IQ = 103, // 0x67

    SPYSERVER_MSG_TYPE_UINT8_AF = 200, // 0xc8
    SPYSERVER_MSG_TYPE_INT16_AF = 201, // 0xc9
    SPYSERVER_MSG_TYPE_INT24_AF = 202, // 0xca
    SPYSERVER_MSG_TYPE_FLOAT_AF = 203, // 0xcb

    SPYSERVER_MSG_TYPE_DINT4_FFT = 300, //0x12C
    SPYSERVER_MSG_TYPE_UINT8_FFT = 301, //0x12D
};

struct SpyServerClientHandshake {
    uint32_t ProtocolVersion;
};

struct SpyServerCommandHeader {
    uint32_t CommandType;
    uint32_t BodySize;
};

struct SpyServerSettingTarget {
    uint32_t Setting;
    uint32_t Value;
};

struct SpyServerMessageHeader {
    uint32_t ProtocolID;
    uint32_t MessageType;
    uint32_t StreamType;
    uint32_t SequenceNumber;
    uint32_t BodySize;
};

struct SpyServerDeviceInfo {
    uint32_t DeviceType;
    uint32_t DeviceSerial;
    uint32_t MaximumSampleRate;
    uint32_t MaximumBandwidth;
    uint32_t DecimationStageCount;
    uint32_t GainStageCount;
    uint32_t MaximumGainIndex;
    uint32_t MinimumFrequency;
    uint32_t MaximumFrequency;
    uint32_t Resolution;
    uint32_t MinimumIQDecimation;
    uint32_t ForcedIQFormat;
};

struct SpyServerClientSync {
    uint32_t CanControl;
    uint32_t Gain;
    uint32_t DeviceCenterFrequency;
    uint32_t IQCenterFrequency;
    uint32_t FFTCenterFrequency;
    uint32_t MinimumIQCenterFrequency;
    uint32_t MaximumIQCenterFrequency;
    uint32_t MinimumFFTCenterFrequency;
    uint32_t MaximumFFTCenterFrequency;
};