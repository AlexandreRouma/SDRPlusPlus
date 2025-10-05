#pragma once
#include <vector>
#include <stdint.h>

namespace backend {
    struct DevVIDPID {
        uint16_t vid;
        uint16_t pid;
    };

    extern const std::vector<DevVIDPID> AIRSPY_VIDPIDS;
    extern const std::vector<DevVIDPID> AIRSPYHF_VIDPIDS;
    extern const std::vector<DevVIDPID> HACKRF_VIDPIDS;
    extern const std::vector<DevVIDPID> HYDRASDR_VIDPIDS;
    extern const std::vector<DevVIDPID> RTL_SDR_VIDPIDS;

    int getDeviceFD(int& vid, int& pid, const std::vector<DevVIDPID>& allowedVidPids);
}