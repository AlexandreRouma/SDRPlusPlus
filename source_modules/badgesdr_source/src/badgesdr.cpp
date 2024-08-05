#include "badgesdr.h"
#include <stdexcept>
#include <utils/flog.h>

#define R820T_I2C_ADDR  0x1A

namespace BadgeSDR {
    enum Commands {
        CMD_I2C_RW,
        CMD_I2C_STATUS,
        CMD_ADC_START,
        CMD_ADC_STOP,
        CMD_ADC_GET_SAMP_COUNT
    };

    libusb_context* ctx = NULL;

    bool DeviceInfo::operator==(const DeviceInfo& b) const {
        return serialNumber == b.serialNumber;
    }

    void Device::write_reg(uint8_t reg, uint8_t value, void* ctx) {
        Device* dev = (Device*)ctx;
        dev->writeR820TReg(reg, value);
        dev->writeR820TReg(0x1F, 0); // TODO: Figure out why this is needed
    }

    void Device::read_reg(uint8_t* data, int len, void* ctx) {
        Device* dev = (Device*)ctx;
        dev->readI2C(R820T_I2C_ADDR, data, len);
    }

    Device::Device(libusb_device_handle* dev) {
        // Save device handle
        this->dev = dev;

        // Init tuner
        r820t = {
            16000000, // xtal_freq => 16MHz
            3000000, // Set at boot to airspy_m0_m4_conf_t conf0 -> r820t_if_freq
            100000000, /* Default Freq 100Mhz */
            {
                /* 05 */ 0x9F, // LNA manual gain mode, init to 0
                /* 06 */ 0x80,
                /* 07 */ 0x60,
                /* 08 */ 0x80, // Image Gain Adjustment
                /* 09 */ 0x40, // Image Phase Adjustment
                /* 0A */ 0xA8, // Channel filter [0..3]: 0 = widest, f = narrowest - Optimal. Don't touch!
                /* 0B */ 0x0F, // High pass filter - Optimal. Don't touch!
                /* 0C */ 0x4F, // VGA control by code, init at 0
                /* 0D */ 0x63, // LNA AGC settings: [0..3]: Lower threshold; [4..7]: High threshold
                /* 0E */ 0x75,
                /* 0F */ 0xE8, // Filter Widest, LDO_5V OFF, clk out ON,
                /* 10 */ 0x7C,
                /* 11 */ 0x42,
                /* 12 */ 0x06,
                /* 13 */ 0x00,
                /* 14 */ 0x0F,
                /* 15 */ 0x00,
                /* 16 */ 0xC0,
                /* 17 */ 0xA0,
                /* 18 */ 0x48,
                /* 19 */ 0xCC,
                /* 1A */ 0x60,
                /* 1B */ 0x00,
                /* 1C */ 0x54,
                /* 1D */ 0xAE,
                /* 1E */ 0x0A,
                /* 1F */ 0xC0
            },
            0 /* uint16_t padding */
        };
        r820t_init(&r820t, 3000000, write_reg, read_reg, this);
        r820t_set_mixer_gain(&r820t, 15);
        r820t_set_vga_gain(&r820t, 15);
    }

    Device::~Device() {
        // Release the bulk interface
        libusb_release_interface(dev, 0);

        // Close device
        libusb_close(dev);
    }

    void Device::setFrequency(double freq) {
        r820t_set_freq(&r820t, freq - 2125000);
    }

    void Device::setLNAGain(int gain) {
        r820t_set_lna_gain(&r820t, gain);
    }

    void Device::setMixerGain(int gain) {
        r820t_set_mixer_gain(&r820t, gain);
    }

    void Device::setVGAGain(int gain) {
        r820t_set_vga_gain(&r820t, gain);
    }


    void Device::start(void (*callback)(const uint8_t* samples, int count, void* ctx), void* ctx, int minBufferSize) {
        // Do nothing if already running
        if (run) { return; }

        // Save handler
        this->callback = callback;
        this->ctx = ctx;

        // Compute buffer size
        int bufCount = minBufferSize / 64;
        if (minBufferSize % 64) { bufCount++; }
        bufferSize = bufCount * 64;

        // Mark as running
        run = true;

        // Start the ADC
        startADC();

        // Start worker thread
        workerThread = std::thread(&Device::worker, this);
    }

    void Device::stop() {
        // Do nothing if already stopped
        if (!run) { return; }

        // Mark as stopped
        run = false;

        // Wait for the worker to exit
        if (workerThread.joinable()) { workerThread.join(); }

        // Stop the ADC
        stopADC();
    }

    int Device::getI2CStatus() {
        int status;
        int ret = libusb_control_transfer(dev, (1 << 7) | (2 << 5), CMD_I2C_STATUS, 0, 0, (uint8_t*)&status, sizeof(status), 1000);
        if (ret <= 0 || status < 0) {
            return -1;
        }
        return status;
    }

    int Device::readI2C(uint8_t addr, uint8_t* data, int len) {
        // Do read
        int bytes = libusb_control_transfer(dev, (1 << 7) | (2 << 5), CMD_I2C_RW, 0, addr, data, len, 1000);
        if (bytes < 0) {
            return -1;
        }

        // Get status (TODO: Use function)
        int status;
        int ret = libusb_control_transfer(dev, (1 << 7) | (2 << 5), CMD_I2C_STATUS, 0, 0, (uint8_t*)&status, sizeof(status), 1000);
        if (ret <= 0 || status < 0) {
            return -1;
        }

        return bytes;
    }

    int Device::writeI2C(uint8_t addr, const uint8_t* data, int len) {
        // Do write
        int bytes = libusb_control_transfer(dev, (2 << 5), CMD_I2C_RW, 0, addr, (uint8_t*)data, len, 1000);
        if (bytes < 0) {
            return -1;
        }

        // Get status (TODO: Use function)
        int status;
        int ret = libusb_control_transfer(dev, (1 << 7) | (2 << 5), CMD_I2C_STATUS, 0, 0, (uint8_t*)&status, sizeof(status), 1000);
        if (ret <= 0 || status < 0) {
            return -1;
        }

        return bytes;
    }

    uint8_t bitrev(uint8_t val) {
        return ((val & 0x01) << 7) | ((val & 0x02) << 5) | ((val & 0x04) << 3) | ((val & 0x08) << 1) | ((val & 0x10) >> 1) | ((val & 0x20) >> 3) | ((val & 0x40) >> 5) | ((val & 0x80) >> 7);
    }

    uint8_t Device::readR820TReg(uint8_t reg) {
        // Read registers up to it (can't read single)
        uint8_t regs[0x20];
        readI2C(R820T_I2C_ADDR, regs, reg+1);

        // Invert bit order
        return bitrev(regs[reg]);
    }

    void Device::writeR820TReg(uint8_t reg, uint8_t val) {
        // Write register id then value
        uint8_t cmd[2] = { reg, val };
        writeI2C(R820T_I2C_ADDR, cmd, 2);
    }

    int Device::startADC() {
        return libusb_control_transfer(dev, (2 << 5), CMD_ADC_START, 0, 0, NULL, 0, 1000);
    }

    int Device::stopADC() {
        return libusb_control_transfer(dev, (2 << 5), CMD_ADC_STOP, 0, 0, NULL, 0, 1000);
    }

    void Device::worker() {
        // Allocate sample buffer
        uint8_t* buffer = new uint8_t[bufferSize];
        int sampleCount = 0;

        while (run) {
            // Receive data with bulk transfer
            int recvLen = 0;
            int val = libusb_bulk_transfer(dev, LIBUSB_ENDPOINT_IN | 1, &buffer[sampleCount], bufferSize - sampleCount, &recvLen, 1000);
            
            // If timed out, try again. Otherwise, if an error occur, stop the thread
            if (val == LIBUSB_ERROR_TIMEOUT) {
                continue;
            }
            else if (val) {
                flog::error("USB Error: {}", val);
                break;
            }

            // Increment sample count
            if (recvLen) { sampleCount += recvLen; }

            // If the buffer is full, call handler and reset sample count
            if (sampleCount >= bufferSize) {
                callback(buffer, sampleCount, ctx);
                sampleCount = 0;
            }
        }

        // Free buffer
        delete[] buffer;
    }

    std::vector<DeviceInfo> list() {
        // Init libusb if done yet
        if (!ctx) {
            libusb_init(&ctx);
            libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_WARNING);
        }

        // List devices
        std::vector<DeviceInfo> devList;
        libusb_device** devices;
        int devCount = libusb_get_device_list(ctx, &devices);
        for (int i = 0; i < devCount; i++) {
            // Get device info
            DeviceInfo devInfo;
            devInfo.dev = devices[i];
            libusb_device_descriptor desc;
            libusb_get_device_descriptor(devInfo.dev, &desc);

            // Check the VID/PID and give up if not the right ones
            if (desc.idVendor != 0xCAFE || desc.idProduct != 0x4010) {
                libusb_unref_device(devInfo.dev);
                continue;
            }
            
            // Open devices
            libusb_device_handle* openDev;
            int err = libusb_open(devInfo.dev, &openDev);
            if (err) {
                libusb_unref_device(devInfo.dev);
                continue;
            }

            // Get serial number
            char serial[128];
            libusb_get_string_descriptor_ascii(openDev, desc.iSerialNumber, (uint8_t*)serial, sizeof(serial));
            devInfo.serialNumber = serial;

            // TODO: The libusb device should be unreffed but would need to know when the list disappears

            // Close device
            libusb_close(openDev);

            // Add to list
            devList.push_back(devInfo);
        }

        // Return devices
        return devList;
    }

    std::shared_ptr<Device> open(const DeviceInfo& dev) {
        // Open device
        libusb_device_handle* openDev;
        int err = libusb_open(dev.dev, &openDev);
        if (err) {
            throw std::runtime_error("Failed to open device");
        }

        // Claim the bulk transfer interface
        if (libusb_claim_interface(openDev, 0)) {
            throw std::runtime_error("Failed to claim bulk interface");
        }

        // Create device
        return std::make_shared<Device>(openDev);
    }
}