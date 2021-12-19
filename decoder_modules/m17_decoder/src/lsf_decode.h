#pragma once
#include <string>
#include <stdint.h>

enum M17DataType {
    M17_DATATYPE_UNKNOWN = 0b00,
    M17_DATATYPE_DATA = 0b01,
    M17_DATATYPE_VOICE = 0b10,
    M17_DATATYPE_DATA_VOICE = 0b11
};

enum M17EncryptionType {
    M17_ENCRYPTION_NONE = 0b00,
    M17_ENCRYPTION_AES = 0b01,
    M17_ENCRYPTION_SCRAMBLE = 0b10,
    M17_ENCRYPTION_UNKNOWN = 0b11
};

extern const char* M17DataTypesTxt[4];
extern const char* M17EncryptionTypesTxt[4];

struct M17LSF {
    uint64_t rawDst;
    uint64_t rawSrc;
    uint16_t rawType;
    uint8_t meta[14];
    uint16_t rawCRC;

    std::string dst;
    std::string src;
    bool isStream;
    M17DataType dataType;
    M17EncryptionType encryptionType;
    uint8_t encryptionSubType;
    uint8_t channelAccessNum;

    bool valid;
};

bool M17CheckCRC(uint8_t* data, int len);

M17LSF M17DecodeLSF(uint8_t* _lsf);