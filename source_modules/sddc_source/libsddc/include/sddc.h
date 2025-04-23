#pragma once
#include <stdint.h>
#include <stdbool.h>

// Handle inclusion from C++ code
#ifdef __cplusplus
extern "C" {
#endif

#define SDDC_SERIAL_MAX_LEN 256

enum sddc_model {
    SDDC_MODEL_UNKNOWN      = 0x00,
    SDDC_MODEL_BBRF103      = 0x01,
    SDDC_MODEL_HF103        = 0x02,
    SDDC_MODEL_RX888        = 0x03,
    SDDC_MODEL_RX888_MK2    = 0x04,
    SDDC_MODEL_RX999        = 0x05,
    SDDC_MODEL_RXLUCY       = 0x06,
    SDDC_MODEL_RX888_MK3    = 0x07
};
typedef enum sddc_model sddc_model_t;

enum sddc_error {
    SDDC_ERROR_UNKNOWN                  = -99,
    SDDC_ERROR_NOT_IMPLEMENTED          = -98,

    SDDC_ERROR_FIRMWARE_UPLOAD_FAILED   = -4,
    SDDC_ERROR_NOT_FOUND                = -3,
    SDDC_ERROR_USB_ERROR                = -2,
    SDDC_ERROR_TIMEOUT                  = -1,

    SDDC_SUCCESS                        =  0
};
typedef enum sddc_error sddc_error_t;

/**
 * Device instance.
*/
struct sddc_dev;
typedef struct sddc_dev sddc_dev_t;

/**
 * Device information.
*/
struct sddc_devinfo {
    const char serial[SDDC_SERIAL_MAX_LEN];
    sddc_model_t model;
    int firmwareMajor;
    int firmwareMinor;
};
typedef struct sddc_devinfo sddc_devinfo_t;

/**
 * Parameter range. A step size of zero means infinitely variable.
*/
struct sddc_range {
    double start;
    double end;
    double step;
};
typedef struct sddc_range sddc_range_t;

/**
 * Get the string representation of a device model.
 * @param model Model to get the string representation of.
 * @return String representation of the model.
*/
const char* sddc_model_to_string(sddc_model_t model);

/**
 * Get the string representation of an error.
 * @param model Error to get the string representation of.
 * @return String representation of the error.
*/
const char* sddc_error_to_string(sddc_error_t error);

/**
 * Set the path to the firmware image.
 * @param path Path to the firmware image.
 * @return SDDC_SUCCESS on success or an error code otherwise.
*/
sddc_error_t sddc_set_firmware_path(const char* path);

/**
 * Get a list of connected devices. The returned list has to be freed using `sddc_free_device_list()` if it isn't empty.
 * @param dev_list Pointer to a list of devices.
 * @return Number of devices in the list or an error code if an error occured.
*/
int sddc_get_device_list(sddc_devinfo_t** dev_list);

/**
 * Free a device list returned by `sddc_get_device_list()`. Attempting to free a list returned empty has no effect.
 * @param dev_list Device list to free.
*/
void sddc_free_device_list(sddc_devinfo_t* dev_list);

/**
 * Open a device by its serial number.
 * @param serial Serial number of the device to open.
 * @param dev Pointer to a SDDC device pointer to populate once open.
 * @return SDDC_SUCCESS on success or an error code otherwise.
*/
sddc_error_t sddc_open(const char* serial, sddc_dev_t** dev);

/**
 * Close an opened SDDC device.
 * @param dev SDDC Device to close.
*/
void sddc_close(sddc_dev_t* dev);

/**
 * Get the range of samplerate supported by a device.
 * @param dev SDDC device.
 * @return Range of supported samplerates.
*/
sddc_range_t sddc_get_samplerate_range(sddc_dev_t* dev);

/**
 * Set the device's sampling rate.
 * @param dev SDDC device.
 * @param samplerate Sampling rate.
 * @return SDDC_SUCCESS on success or an error code otherwise.
*/
sddc_error_t sddc_set_samplerate(sddc_dev_t* dev, uint32_t samplerate);

/**
 * Enable the ADC's dithering feature.
 * @param dev SDDC device.
 * @param enabled True to enable dithering, false to disable it.
 * @return SDDC_SUCCESS on success or an error code otherwise.
*/
sddc_error_t sddc_set_dithering(sddc_dev_t* dev, bool enabled);

/**
 * Enable the ADC's randomizer feature.
 * @param dev SDDC device.
 * @param enabled True to enable randomization, false to disable it.
 * @return SDDC_SUCCESS on success or an error code otherwise.
*/
sddc_error_t sddc_set_randomizer(sddc_dev_t* dev, bool enabled);

/**
 * Set the LO of the tuner.
 * @param dev SDDC device.
 * @param frequency Frequency of the LO.
 * @return SDDC_SUCCESS on success or an error code otherwise.
*/
sddc_error_t sddc_set_tuner_frequency(sddc_dev_t* dev, uint64_t frequency);

/**
 * Start the device.
 * @param dev SDDC device.
 * @return SDDC_SUCCESS on success or an error code otherwise.
*/
sddc_error_t sddc_start(sddc_dev_t* dev);

/**
 * Stop the device.
 * @param dev SDDC device.
 * @return SDDC_SUCCESS on success or an error code otherwise.
*/
sddc_error_t sddc_stop(sddc_dev_t* dev);

/**
 * Receive samples.
 * @param dev SDDC device.
 * @param samples Buffer to write the samples to.
 * @param count Number of samples to read.
 * @return SDDC_SUCCESS on success or an error code otherwise.
*/
sddc_error_t sddc_rx(sddc_dev_t* dev, int16_t* samples, int count);

// Handle inclusion from C++ code
#ifdef __cplusplus
}
#endif