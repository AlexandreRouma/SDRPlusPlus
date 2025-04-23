#include "usb_interface.h"
#include "fx3_boot.h"
#include <stdio.h>

sddc_error_t sddc_init_device(libusb_device* dev, const char* firmware) {
    // Open the device
    libusb_device_handle* openDev;
    int err = libusb_open(dev, &openDev);
    if (err != LIBUSB_SUCCESS) {
        printf("Failed to open device: %d\n", err);
        return SDDC_ERROR_USB_ERROR; 
    }

    // Attempt to upload the firmware
    err = sddc_fx3_boot_upload_firmware(openDev, firmware);
    if (err) {
        fprintf(stderr, "Failed to upload firmware to uninitialized device\n");
        return SDDC_ERROR_FIRMWARE_UPLOAD_FAILED;
    }

    // Close the device
    libusb_close(openDev);

    // Return successfully
    return SDDC_SUCCESS;
}

int sddc_fx3_start(libusb_device_handle* dev) {
    // Send the start command
    uint32_t dummy;
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_FX3_START, 0, 0, &dummy, sizeof(uint32_t), SDDC_TIMEOUT_MS);
}

int sddc_fx3_stop(libusb_device_handle* dev) {
    // Send the stop command
    uint32_t dummy;
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_FX3_STOP, 0, 0, &dummy, sizeof(uint32_t), SDDC_TIMEOUT_MS);
}

int sddc_fx3_get_info(libusb_device_handle* dev, sddc_hwinfo_t* info, char enableDebug) {
    // Fetch the info data
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_FX3_GET_INFO, enableDebug, 0, info, sizeof(sddc_hwinfo_t), SDDC_TIMEOUT_MS);
}

int sddc_fx3_gpio(libusb_device_handle* dev, sddc_gpio_t gpio) {
    // Send the GPIO state
    uint32_t dword = gpio;
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_FX3_GPIO, 0, 0, &dword, sizeof(uint32_t), SDDC_TIMEOUT_MS);
}

int sddc_fx3_i2c_write(libusb_device_handle* dev, uint8_t addr, uint16_t reg, const uint8_t* data, uint16_t len) {
    // Send the I2C data
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_FX3_I2C_W, addr, reg, data, len, SDDC_TIMEOUT_MS);
}

int sddc_fx3_i2c_read(libusb_device_handle* dev, uint8_t addr, uint16_t reg, const uint8_t* data, uint16_t len) {
    // Fetch the I2C data
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_FX3_I2C_R, addr, reg, data, len, SDDC_TIMEOUT_MS);
}

int sddc_fx3_reset(libusb_device_handle* dev) {
    // Send the reset command
    uint32_t dummy;
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_FX3_RESET, 0, 0, &dummy, sizeof(uint32_t), SDDC_TIMEOUT_MS);
}

int sddc_fx3_set_param(libusb_device_handle* dev, sddc_param_t param, uint16_t value) {
    // Send the parameter
    uint32_t dummy;
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_FX3_SET_PARAM, value, param, &dummy, sizeof(uint32_t), SDDC_TIMEOUT_MS);
}

int sddc_adc_set_samplerate(libusb_device_handle* dev, uint32_t samplerate) {
    // Send the samplerate
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_ADC_SET_RATE, 0, 0, &samplerate, sizeof(uint32_t), SDDC_TIMEOUT_MS);
}

int sddc_tuner_start(libusb_device_handle* dev, uint32_t refFreq) {
    // Send the reset command
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_TUNER_START, 0, 0, &refFreq, sizeof(uint32_t), SDDC_TIMEOUT_MS);
}

int sddc_tuner_tune(libusb_device_handle* dev, uint64_t frequency) {
    // Send the reset command
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_TUNER_TUNE, 0, 0, &frequency, sizeof(uint64_t), SDDC_TIMEOUT_MS);
}

int sddc_tuner_stop(libusb_device_handle* dev) {
    // Send the reset command
    uint32_t dummy; // Because the shitty firmware absolute wants data to stop the tuner, this is dumb...
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, SDDC_CMD_TUNER_STOP, 0, 0, &dummy, sizeof(uint32_t), SDDC_TIMEOUT_MS);
}