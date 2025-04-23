#pragma once
#include <sddc.h>
#include <stdint.h>
#include <libusb.h>

#define SDDC_UNINIT_VID                 0x04B4
#define SDDC_UNINIT_PID                 0x00F3
#define SDDC_VID                        0x04B4
#define SDDC_PID                        0x00F1

#define SDDC_INIT_SEARCH_DELAY_MS       1000
#define SDDC_TIMEOUT_MS                 1000

enum sddc_command {
    SDDC_CMD_FX3_START      = 0xAA,
    SDDC_CMD_FX3_STOP       = 0xAB,
    SDDC_CMD_FX3_GET_INFO   = 0xAC,
    SDDC_CMD_FX3_GPIO       = 0xAD,
    SDDC_CMD_FX3_I2C_W      = 0xAE,
    SDDC_CMD_FX3_I2C_R      = 0xAF,
    SDDC_CMD_FX3_RESET      = 0xB1,
    SDDC_CMD_FX3_SET_PARAM  = 0xB6,
    SDDC_CMD_ADC_SET_RATE   = 0xB2,
    SDDC_CMD_TUNER_START    = 0xB4,
    SDDC_CMD_TUNER_TUNE     = 0xB5,
    SDDC_CMD_TUNER_STOP     = 0xB8,
    SDDC_CMD_READ_DEBUG     = 0xBA
};

#pragma pack(push, 1)
struct sddc_hwinfo {
    uint8_t model;
    uint8_t firmwareConfigH;
    uint8_t firmwareConfigL;
    uint8_t vendorRequestCount;
};
typedef struct sddc_hwinfo sddc_hwinfo_t;
#pragma pack(pop)

enum sddc_gpio {
    SDDC_GPIO_NONE          = 0,
    SDDC_GPIO_ATT_LE        = (1 << 0),
    SDDC_GPIO_ATT_CLK       = (1 << 1),
    SDDC_GPIO_ATT_DATA      = (1 << 2),
    SDDC_GPIO_SEL0          = (1 << 3),
    SDDC_GPIO_SEL1          = (1 << 4),
    SDDC_GPIO_SHUTDOWN      = (1 << 5),
    SDDC_GPIO_DITHER        = (1 << 6),
    SDDC_GPIO_RANDOM        = (1 << 7),
    SDDC_GPIO_BIAST_HF      = (1 << 8),
    SDDC_GPIO_BIAST_VHF     = (1 << 9),
    SDDC_GPIO_LED_YELLOW    = (1 << 10),
    SDDC_GPIO_LED_RED       = (1 << 11),
    SDDC_GPIO_LED_BLUE      = (1 << 12),
    SDDC_GPIO_ATT_SEL0      = (1 << 13),
    SDDC_GPIO_ATT_SEL1      = (1 << 14),
    SDDC_GPIO_VHF_EN        = (1 << 15),
    SDDC_GPIO_PGA_EN        = (1 << 16),
    SDDC_GPIO_ALL           = ((1 << 17)-1)
};
typedef enum sddc_gpio sddc_gpio_t;

enum sddc_param {
    SDDC_PARAM_R82XX_ATT        = 1,
    SDDC_PARAM_R83XX_VGA        = 2,
    SDDC_PARAM_R83XX_SIDEBAND   = 3,
    SDDC_PARAM_R83XX_HARMONIC   = 4,
    SDDC_PARAM_DAT31_ATT        = 10,
    SDDC_PARAM_AD8340_VGA       = 11,
    SDDC_PARAM_PRESELECTOR      = 12,
    SDDC_PARAM_VHF_ATT          = 13
};
typedef enum sddc_param sddc_param_t;

/**
 * Initialize a device with a given firmware.
 * @param dev SDDC USB device entry to initialize.
 * @param firmware Path to the firmware image.
*/
sddc_error_t sddc_init_device(libusb_device* dev, const char* firmware);

/**
 * Start the FX3 streaming.
 * @param dev SDDC USB device.
*/
int sddc_fx3_start(libusb_device_handle* dev);

/**
 * Stop the FX3 streaming.
 * @param dev SDDC USB device.
*/
int sddc_fx3_stop(libusb_device_handle* dev);

/**
 * Get the hardware info of a device.
 * @param dev SDDC USB device.
 * @param info Hardware info struct to write the return info into.
 * @param enableDebug 1 to enable hardware debug mode, 0 otherwise.
*/
int sddc_fx3_get_info(libusb_device_handle* dev, sddc_hwinfo_t* info, char enableDebug);

/**
 * Set the state of the GPIO pins.
 * @param dev SDDC USB device.
 * @param gpio State of the GPIO pins.
*/
int sddc_fx3_gpio(libusb_device_handle* dev, sddc_gpio_t gpio);

/**
 * Write data to the I2C port.
 * @param dev SDDC USB device.
 * @param addr Address of the target I2C device.
 * @param reg Register to write data to.
 * @param data Data buffer to write.
 * @param len Number of bytes to write.
*/
int sddc_fx3_i2c_write(libusb_device_handle* dev, uint8_t addr, uint16_t reg, const uint8_t* data, uint16_t len);

/**
 * Read data from the I2C port.
 * @param dev SDDC USB device.
 * @param addr Address of the target I2C device.
 * @param reg Register to read data from.
 * @param data Data buffer to read data into.
 * @param len Number of bytes to read.
*/
int sddc_fx3_i2c_write(libusb_device_handle* dev, uint8_t addr, uint16_t reg, const uint8_t* data, uint16_t len);

/**
 * Reset the FX3 into bootloader mode.
 * @param dev SDDC USB device.
*/
int sddc_fx3_reset(libusb_device_handle* dev);

/**
 * Set a device parameter.
 * @param dev SDDC USB device.
 * @param arg Parameter to set.
 * @param value Value to set the parameter to.
*/
int sddc_fx3_set_param(libusb_device_handle* dev, sddc_param_t param, uint16_t value);

/**
 * Set the ADC sampling rate.
 * @param dev SDDC USB device.
 * @param samplerate Sampling rate. 0 to stop the ADC.
*/
int sddc_adc_set_samplerate(libusb_device_handle* dev, uint32_t samplerate);

/**
 * Start the tuner.
 * @param dev SDDC USB device.
 * @param frequency Initial LO frequency.
*/
int sddc_tuner_start(libusb_device_handle* dev, uint32_t refFreq);

/**
 * Set the tuner's LO frequency.
 * @param dev SDDC USB device.
 * @param frequency New LO frequency.
*/
int sddc_tuner_tune(libusb_device_handle* dev, uint64_t frequency);

/**
 * Stop the tuner.
 * @param dev SDDC USB device.
*/
int sddc_tuner_stop(libusb_device_handle* dev);