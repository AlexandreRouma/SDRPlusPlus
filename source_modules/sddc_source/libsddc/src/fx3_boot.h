#pragma once
#include <libusb.h>
#include <stdint.h>

/**
 * Read data from the device's memory.
 * @param dev Device to read data from.
 * @param addr Start address of the data in the device's memory.
 * @param data Buffer to write the data into.
 * @param len Number of bytes to read.
 * @return libusb error code.
*/
int sddc_fx3_boot_mem_read(libusb_device_handle* dev, uint32_t addr, uint8_t* data, uint16_t len);

/**
 * Write data to the device's memory.
 * @param dev Device to write data to.
 * @param addr Start address of the data in the device's memory.
 * @param data Buffer to write the data into.
 * @param len Number of bytes to read.
 * @return libusb error code.
*/
int sddc_fx3_boot_mem_write(libusb_device_handle* dev, uint32_t addr, const uint8_t* data, uint16_t len);

/**
 * Execute code on the device.
 * @param dev Device to execute code on.
 * @param entry Entry point of the code.
 * @return libusb error code.
*/
int sddc_fx3_boot_run(libusb_device_handle* dev, uint32_t entry);

/**
 * Parse, upload and execute a firmware image.
 * @param dev Device to upload the firmware to.
 * @param path Path to the firmware image.
 * @return libusb error code.
*/
int sddc_fx3_boot_upload_firmware(libusb_device_handle* dev, const char* path);