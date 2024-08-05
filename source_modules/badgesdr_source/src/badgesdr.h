#pragma once
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <libusb.h>
#include "r820t.h"

namespace BadgeSDR {
    struct DeviceInfo {
        std::string serialNumber;
        libusb_device* dev;
        bool operator==(const DeviceInfo& b) const;
    };

    class Device {
    public:
        Device(libusb_device_handle* dev);
        ~Device();

        void setFrequency(double freq);
        void setLNAGain(int gain);
        void setMixerGain(int gain);
        void setVGAGain(int gain);
        
        void start(void (*callback)(const uint8_t* samples, int count, void* ctx), void* ctx = NULL, int minBufferSize = 2500);
        void stop();

    private:
        int getI2CStatus();
        int readI2C(uint8_t addr, uint8_t* data, int len);
        int writeI2C(uint8_t addr, const uint8_t* data, int len);
        uint8_t readR820TReg(uint8_t reg);
        void writeR820TReg(uint8_t reg, uint8_t val);
        int startADC();
        int stopADC();
        void worker();

        libusb_device_handle* dev;
        std::thread workerThread;
        bool run = false;
        int bufferSize = 0; // Must be multiple of 64 for best performance
        void* ctx = NULL;
        void (*callback)(const uint8_t* samples, int count, void* ctx);

        static void write_reg(uint8_t reg, uint8_t value, void* ctx);
        static void read_reg(uint8_t* data, int len, void* ctx);
        r820t_priv_t r820t;
    };

    std::vector<DeviceInfo> list();
    std::shared_ptr<Device> open(const DeviceInfo& dev);
}