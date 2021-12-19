#include <base40.h>

void decode_callsign_base40(uint64_t encoded, char* callsign) {
    if (encoded >= 262144000000000) { // 40^9
        *callsign = 0;
        return;
    }
    char* p = callsign;
    for (; encoded > 0; p++) {
        *p = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."[encoded % 40];
        encoded /= 40;
    }
    *p = 0;

    return;
}