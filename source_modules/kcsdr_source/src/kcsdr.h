#pragma once
#include <stdint.h>

#define KCSDR_SERIAL_LEN    16
#define KCSDR_MAX_PORTS     6

// Detect C++
#ifdef __cplusplus
extern "C" {
#endif

/**
 * KCSDR Device.
*/
struct kcsdr;
typedef struct kcsdr kcsdr_t;

/**
 * Device Information
*/
struct kcsdr_info {
    char serial[KCSDR_SERIAL_LEN+1];
};
typedef struct kcsdr_info kcsdr_info_t;

/**
 * RF Direction.
*/
enum kcsdr_direction {
    KCSDR_DIR_RX    = 0x00,
    KCSDR_DIR_TX    = 0x80
};
typedef enum kcsdr_direction kcsdr_direction_t;

/**
 * Get a list of KCSDR devices on the system.
 * @param devices Pointer to an array of device info.
 * @return Number of devices found or error code.
*/
int kcsdr_list_devices(kcsdr_info_t** devices);

/**
 * Free a device list returned by `kcsdr_list_devices()`.
 * @param devices Device list to free.
*/
void kcsdr_free_device_list(kcsdr_info_t* devices);

/**
 * Open a KCSDR device.
 * @param dev Newly open device.
 * @param serial Serial number of the device to open as returned in the device list.
 * @return 0 on success, error code otherwise.
*/
int kcsdr_open(kcsdr_t** dev, const char* serial);

/**
 * Close a KCSDR device.
 * @param dev Device to be closed.
*/
void kcsdr_close(kcsdr_t* dev);

/**
 * Select the RF port.
 * @param dev Device to control.
 * @param dir Either KCSDR_DIR_RX or KCSDR_DIR_TX.
 * @param port RF port number to select.
 * @return 0 on success, error code otherwise.
*/
int kcsdr_set_port(kcsdr_t* dev, kcsdr_direction_t dir, uint8_t port);

/**
 * Set the center frequency.
 * @param dev Device to control.
 * @param dir Either KCSDR_DIR_RX or KCSDR_DIR_TX
 * @param freq Frequency in Hz.
 * @return 0 on success, error code otherwise.
*/
int kcsdr_set_frequency(kcsdr_t* dev, kcsdr_direction_t dir, uint64_t freq);

/**
 * Set the attenuation.
 * @param dev Device to control.
 * @param dir Either KCSDR_DIR_RX or KCSDR_DIR_TX
 * @param samplerate Attenuation in dB.
 * @return 0 on success, error code otherwise.
*/
int kcsdr_set_attenuation(kcsdr_t* dev, kcsdr_direction_t dir, uint8_t att);

/**
 * Set the internal amplifier gain.
 * @param dev Device to control.
 * @param dir Either KCSDR_DIR_RX or KCSDR_DIR_TX
 * @param gain Gain in dB.
 * @return 0 on success, error code otherwise.
*/
int kcsdr_set_amp_gain(kcsdr_t* dev, kcsdr_direction_t dir, uint8_t gain);

/**
 * Set the external amplifier gain.
 * @param dev Device to control.
 * @param gain Gain in dB.
 * @return 0 on success, error code otherwise.
*/
int kcsdr_set_rx_ext_amp_gain(kcsdr_t* dev, uint8_t gain);

/**
 * Set the samplerate.
 * @param dev Device to control.
 * @param dir Either KCSDR_DIR_RX or KCSDR_DIR_TX
 * @param samplerate Samplerate in Hz.
 * @return 0 on success, error code otherwise.
*/
int kcsdr_set_samplerate(kcsdr_t* dev, kcsdr_direction_t dir, uint32_t samplerate);

/**
 * Start streaming samples.
 * @param dev Device to control.
 * @param dir Either KCSDR_DIR_RX or KCSDR_DIR_TX.
 * @return 0 on success, error code otherwise.
*/
int kcsdr_start(kcsdr_t* dev, kcsdr_direction_t dir);

/**
 * Stop streaming samples.
 * @param dev Device to control.
 * @param dir Either KCSDR_DIR_RX or KCSDR_DIR_TX.
 * @return 0 on success, error code otherwise.
*/
int kcsdr_stop(kcsdr_t* dev, kcsdr_direction_t dir);

/**
 * Receive a buffer of samples.
 * @param samples Sample buffer.
 * @param count Number of complex samples.
 * @return Number of samples received.
*/
int kcsdr_rx(kcsdr_t* dev, int16_t* samples, int count);

/**
 * Transmit a buffer of samples.
 * @param samples Sample buffer.
 * @param count Number of complex samples.
 * @return Number of samples transmitted.
*/
int kcsdr_tx(kcsdr_t* dev, const int16_t* samples, int count);

// Detect C++
#ifdef __cplusplus
}
#endif
