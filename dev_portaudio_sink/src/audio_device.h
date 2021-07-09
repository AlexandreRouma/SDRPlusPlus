#pragma once

struct AudioDevice_t {
    std::string name;
    int deviceId;
    int channels;
    int sampleRateId;
    std::vector<float> sampleRates;
};