#include <lsf_decode.h>
#include <string.h>
#include <base40.h>
#include <stdio.h>
#include <inttypes.h>
#include <crc16.h>
#include <spdlog/spdlog.h>

bool M17CheckCRC(uint8_t* data, int len) {
    // TODO: Implement
    return true;
}

const char* M17DataTypesTxt[4] = {
    "Unknown",
    "Data",
    "Voice",
    "Voice & Data"
};

const char* M17EncryptionTypesTxt[4] = {
    "None",
    "AES",
    "Scrambler",
    "Unknown"
};

M17LSF M17DecodeLSF(uint8_t* _lsf) {
    M17LSF lsf;

    // Extract CRC
    lsf.rawCRC = 0;
    for (int i = 0; i < 16; i++) {
        lsf.rawCRC |= (((uint16_t)_lsf[(i + 48 + 48 + 16 + 112) / 8] >> (7 - (i % 8))) & 1) << (15 - i);
    }

    // Check CRC
    mobilinkd::CRC16 crc16;
    crc16.reset();
    for (int i = 0; i < 28; i++) {
        crc16(_lsf[i]);
    }
    if (crc16.get() != lsf.rawCRC) {
        lsf.valid = false;
        return lsf;
    }
    lsf.valid = true;


    // Extract DST
    lsf.rawDst = 0;
    for (int i = 0; i < 48; i++) {
        lsf.rawDst |= (((uint64_t)_lsf[i / 8] >> (7 - (i % 8))) & 1) << (47 - i);
    }

    // Extract SRC
    lsf.rawSrc = 0;
    for (int i = 0; i < 48; i++) {
        lsf.rawSrc |= (((uint64_t)_lsf[(i + 48) / 8] >> (7 - (i % 8))) & 1) << (47 - i);
    }

    // Extract TYPE
    lsf.rawType = 0;
    for (int i = 0; i < 16; i++) {
        lsf.rawType |= (((uint16_t)_lsf[(i + 48 + 48) / 8] >> (7 - (i % 8))) & 1) << (15 - i);
    }

    // Extract META
    memcpy(lsf.meta, &_lsf[14], 14);

    // Decode DST
    if (lsf.rawDst == 0) {
        lsf.dst = "Invalid";
    }
    else if (lsf.rawDst <= 262143999999999) {
        char buf[128];
        decode_callsign_base40(lsf.rawDst, buf);
        lsf.dst = buf;
    }
    else if (lsf.rawDst == 0xFFFFFFFFFFFF) {
        lsf.dst = "Broadcast";
    }
    else {
        char buf[128];
        sprintf(buf, "%" PRIX64, lsf.rawDst);
        lsf.dst = buf;
    }

    // Decode SRC
    if (lsf.rawSrc == 0 || lsf.rawSrc == 0xFFFFFFFFFFFF) {
        lsf.src = "Invalid";
    }
    else if (lsf.rawSrc <= 262143999999999) {
        char buf[128];
        decode_callsign_base40(lsf.rawSrc, buf);
        lsf.src = buf;
    }
    else {
        char buf[128];
        sprintf(buf, "%" PRIX64, lsf.rawSrc);
        lsf.src = buf;
    }

    // Decode TYPE
    lsf.isStream = (lsf.rawType >> 0) & 0b1;
    lsf.dataType = (M17DataType)((lsf.rawType >> 1) & 0b11);
    lsf.encryptionType = (M17EncryptionType)((lsf.rawType >> 3) & 0b11);
    lsf.encryptionSubType = (lsf.rawType >> 5) & 0b11;
    lsf.channelAccessNum = (lsf.rawType >> 7) & 0b1111;

    return lsf;
}