#include <sddc.h>
#include <stdlib.h>
#include <stdint.h>
#include <libusb.h>
#include <stdio.h>
#include "usb_interface.h"

struct sddc_dev {
    // USB handles
    struct libusb_device_handle* openDev;

    // Device info
    sddc_devinfo_t info;

    // Device state
    bool running;
    uint32_t samplerate;
    uint64_t tunerFreq;
    sddc_gpio_t gpioState;
};

struct libusb_context* ctx = NULL;
char* sddc_firmware_path = NULL;
bool sddc_is_init = false;

void sddc_init() {
    // If already initialized, do nothing
    if (sddc_is_init) { return; }

    // If the firmware isn't already found, find it
    if (!sddc_firmware_path) {
        // TODO: Find the firmware
    }

    // Init libusb
    libusb_init(&ctx);
}

const char* sddc_model_to_string(sddc_model_t model) {
    switch (model) {
    case SDDC_MODEL_BBRF103:    return "BBRF103";
    case SDDC_MODEL_HF103:      return "HF103";
    case SDDC_MODEL_RX888:      return "RX888";
    case SDDC_MODEL_RX888_MK2:  return "RX888 MK2";
    case SDDC_MODEL_RX999:      return "RX999";
    case SDDC_MODEL_RXLUCY:     return "RXLUCY";
    case SDDC_MODEL_RX888_MK3:  return "RX888 MK3";
    default:                    return "Unknown";
    }
}

const char* sddc_error_to_string(sddc_error_t error) {
    switch (error) {
    case SDDC_ERROR_NOT_IMPLEMENTED:        return "Not Implemented";
    case SDDC_ERROR_FIRMWARE_UPLOAD_FAILED: return "Firmware Upload Failed";
    case SDDC_ERROR_NOT_FOUND:              return "Not Found";
    case SDDC_ERROR_USB_ERROR:              return "USB Error";
    case SDDC_ERROR_TIMEOUT:                return "Timeout";
    case SDDC_SUCCESS:                      return "Success";
    default:                                return "Unknown";
    }
}

sddc_error_t sddc_set_firmware_path(const char* path) {
    // Free the old path if it exists
    if (sddc_firmware_path) { free(sddc_firmware_path); }

    // Allocate the new path
    sddc_firmware_path = malloc(strlen(path) + 1);

    // Copy the new path
    strcpy(sddc_firmware_path, path);

    // TODO: Check if the file path exists
    return SDDC_SUCCESS;
}

int sddc_get_device_list(sddc_devinfo_t** dev_list) {
    // Initialize libsddc in case it isn't already
    sddc_init();

    // Get a list of USB devices
    libusb_device** devices;
    int devCount = libusb_get_device_list(ctx, &devices);
    if (devCount < 0 || !devices) { return SDDC_ERROR_USB_ERROR; }

    // Initialize all uninitialized devices
    bool uninit = false;
    for (int i = 0; i < devCount; i++) {
        // Get the device from the list
        libusb_device* dev = devices[i];

        // Get the device descriptor. Fail silently on error as it might not even be a SDDC device.
        struct libusb_device_descriptor desc;
        int err = libusb_get_device_descriptor(dev, &desc);
        if (err != LIBUSB_SUCCESS) { continue; }

        // If it's not an uninitialized device, go to next device
        if (desc.idVendor != SDDC_UNINIT_VID || desc.idProduct != SDDC_UNINIT_PID) { continue; }

        // Initialize the device
        printf("Found uninitialized device, initializing...\n");
        // TODO: Check that the firmware path is valid
        sddc_error_t serr = sddc_init_device(dev, sddc_firmware_path);
        if (serr != SDDC_SUCCESS) { continue; }

        // Set the flag to wait the devices to start up
        uninit = true;
    }

    // If some uninitialized devices were found
    if (uninit) {
        // Free the device list
        libusb_free_device_list(devices, 1);

        // Wait for the devices to show back up
#ifdef _WIN32
        Sleep(SDDC_INIT_SEARCH_DELAY_MS);
#else
        usleep(SDDC_INIT_SEARCH_DELAY_MS * 1000);
#endif

        // Attempt to list devices again
        devCount = libusb_get_device_list(ctx, &devices);
        if (devCount < 0 || !devices) { return SDDC_ERROR_USB_ERROR; }
    }

    // Allocate the device list
    *dev_list = malloc(devCount * sizeof(sddc_devinfo_t));

    // Check each device
    int found = 0;
    libusb_device_handle* openDev;
    for (int i = 0; i < devCount; i++) {
        // Get the device from the list
        libusb_device* dev = devices[i];

        // Get the device descriptor. Fail silently on error as it might not even be a SDDC device.
        struct libusb_device_descriptor desc;
        int err = libusb_get_device_descriptor(dev, &desc);
        if (err != LIBUSB_SUCCESS) { continue; }

        // If the device is not an SDDC device, go to next device
        if (desc.idVendor != SDDC_VID || desc.idProduct != SDDC_PID) { continue; }

        // Open the device
        err = libusb_open(dev, &openDev);
        if (err != LIBUSB_SUCCESS) {
            fprintf(stderr, "Failed to open device: %d\n", err);
            continue; 
        }

        // Create entry
        sddc_devinfo_t* info = &((*dev_list)[found]);

        // Get the serial number
        err = libusb_get_string_descriptor_ascii(openDev, desc.iSerialNumber, info->serial, SDDC_SERIAL_MAX_LEN-1);
        if (err < LIBUSB_SUCCESS) {
            printf("Failed to get descriptor: %d\n", err);
            libusb_close(openDev);
            continue;
        }

        // Get the hardware info
        sddc_hwinfo_t hwinfo;
        err = sddc_fx3_get_info(openDev, &hwinfo, 0);
        if (err < LIBUSB_SUCCESS) {
            printf("Failed to get device info: %d\n", err);
            libusb_close(openDev);
            continue;
        }

        // Save the hardware info
        info->model = (sddc_model_t)hwinfo.model;
        info->firmwareMajor = hwinfo.firmwareConfigH;
        info->firmwareMinor = hwinfo.firmwareConfigL;

        // Close the device
        libusb_close(openDev);

        // Increment device counter
        found++;
    }

    // Free the libusb device list
    libusb_free_device_list(devices, 1);

    // Return the number of devices found
    return found;
}

void sddc_free_device_list(sddc_devinfo_t* dev_list) {
    // Free the device list if it exists
    if (dev_list) { free(dev_list); };
}

sddc_error_t sddc_open(const char* serial, sddc_dev_t** dev) {
    // Initialize libsddc in case it isn't already
    sddc_init();

    // Get a list of USB devices
    libusb_device** devices;
    int devCount = libusb_get_device_list(ctx, &devices);
    if (devCount < 0 || !devices) { return SDDC_ERROR_USB_ERROR; }

    // Initialize all uninitialized devices
    bool uninit = false;
    for (int i = 0; i < devCount; i++) {
        // Get the device from the list
        libusb_device* dev = devices[i];

        // Get the device descriptor. Fail silently on error as it might not even be a SDDC device.
        struct libusb_device_descriptor desc;
        int err = libusb_get_device_descriptor(dev, &desc);
        if (err != LIBUSB_SUCCESS) { continue; }

        // If it's not an uninitialized device, go to next device
        if (desc.idVendor != SDDC_UNINIT_VID || desc.idProduct != SDDC_UNINIT_PID) { continue; }

        // Initialize the device
        printf("Found uninitialized device, initializing...\n");
        // TODO: Check that the firmware path is valid
        sddc_error_t serr = sddc_init_device(dev, sddc_firmware_path);
        if (serr != SDDC_SUCCESS) { continue; }

        // Set the flag to wait the devices to start up
        uninit = true;
    }

    // If some uninitialized devices were found
    if (uninit) {
        // Free the device list
        libusb_free_device_list(devices, 1);

        // Wait for the devices to show back up
#ifdef _WIN32
        Sleep(SDDC_INIT_SEARCH_DELAY_MS);
#else
        usleep(SDDC_INIT_SEARCH_DELAY_MS * 1000);
#endif

        // Attempt to list devices again
        devCount = libusb_get_device_list(ctx, &devices);
        if (devCount < 0 || !devices) { return SDDC_ERROR_USB_ERROR; }
    }

    // Search through all USB device
    bool found = false;
    libusb_device_handle* openDev;
    for (int i = 0; i < devCount; i++) {
        // Get the device from the list
        libusb_device* dev = devices[i];

        // Get the device descriptor. Fail silently on error as it might not even be a SDDC device.
        struct libusb_device_descriptor desc;
        int err = libusb_get_device_descriptor(dev, &desc);
        if (err != LIBUSB_SUCCESS) { continue; }

        // If the device is not an SDDC device, go to next device
        if (desc.idVendor != SDDC_VID || desc.idProduct != SDDC_PID) { continue; }

        // Open the device
        err = libusb_open(dev, &openDev);
        if (err != LIBUSB_SUCCESS) {
            fprintf(stderr, "Failed to open device: %d\n", err);
            continue; 
        }

        // Get the serial number
        char dserial[SDDC_SERIAL_MAX_LEN];
        err = libusb_get_string_descriptor_ascii(openDev, desc.iSerialNumber, dserial, SDDC_SERIAL_MAX_LEN-1);
        if (err < LIBUSB_SUCCESS) {
            printf("Failed to get descriptor: %d\n", err);
            libusb_close(openDev);
            continue;
        }

        // Compare the serial number and give up if not a match
        if (strcmp(dserial, serial)) { continue; }

        // Get the device info
        // TODO

        // Set the found flag and stop searching
        found = true;
        break;
    }

    // Free the libusb device list
    libusb_free_device_list(devices, true);

    // If the device was not found, give up
    if (!found) { return SDDC_ERROR_NOT_FOUND; }

    // Claim the interface
    libusb_claim_interface(openDev, 0);

    // Allocate the device object
    *dev = malloc(sizeof(sddc_dev_t));

    // Initialize the device object
    (*dev)->openDev = openDev;
    //(*dev)->info = ; //TODO
    (*dev)->running = false;
    (*dev)->samplerate = 128e6;
    (*dev)->tunerFreq = 100e6;
    (*dev)->gpioState = SDDC_GPIO_SHUTDOWN | SDDC_GPIO_SEL0; // ADC shutdown and HF port selected
    
    // Stop everything in case the device is partially started
    printf("Stopping...\n");
    sddc_stop(*dev);

    // TODO: Setup all of the other state
    sddc_gpio_put(*dev, SDDC_GPIO_SEL0, false);
    sddc_gpio_put(*dev, SDDC_GPIO_SEL1, true);
    sddc_gpio_put(*dev, SDDC_GPIO_VHF_EN, true);
    sddc_tuner_start((*dev)->openDev, 16e6);
    sddc_tuner_tune((*dev)->openDev, 100e6);
    sddc_fx3_set_param((*dev)->openDev, SDDC_PARAM_R82XX_ATT, 15);
    sddc_fx3_set_param((*dev)->openDev, SDDC_PARAM_R83XX_VGA, 9);
    sddc_fx3_set_param((*dev)->openDev, SDDC_PARAM_AD8340_VGA, 5);

    return SDDC_SUCCESS;
}

void sddc_close(sddc_dev_t* dev) {
    // Stop everything
    sddc_stop(dev);

    // Release the interface
    libusb_release_interface(dev->openDev, 0);

    // Close the USB device
    libusb_close(dev->openDev);

    // Free the device struct
    free(dev);
}

sddc_range_t sddc_get_samplerate_range(sddc_dev_t* dev) {
    // All devices have the same samplerate range
    sddc_range_t range = { 8e6, 128e6, 0 };
    return range;
}

int sddc_gpio_set(sddc_dev_t* dev, sddc_gpio_t gpios) {
    // Update the state
    dev->gpioState = gpios;

    // Push to the device
    sddc_fx3_gpio(dev->openDev, gpios);
}

int sddc_gpio_put(sddc_dev_t* dev, sddc_gpio_t gpios, bool value) {
    // Update the state of the given GPIOs only
    return sddc_gpio_set(dev, (dev->gpioState & (~gpios)) | (value ? gpios : 0));
}

sddc_error_t sddc_set_samplerate(sddc_dev_t* dev, uint32_t samplerate) {
    // Update the state
    dev->samplerate = samplerate;

    // If running, send the new sampling rate to the device
    if (dev->running) {
        int err = sddc_adc_set_samplerate(dev->openDev, samplerate);
        if (err < LIBUSB_SUCCESS) { return SDDC_ERROR_USB_ERROR; }
    }

    // Return successfully
    return SDDC_SUCCESS;
}

sddc_error_t sddc_set_dithering(sddc_dev_t* dev, bool enabled) {
    // Update the GPIOs according to the desired state
    int err = sddc_gpio_put(dev, SDDC_GPIO_DITHER, enabled);
    return (err < LIBUSB_SUCCESS) ? SDDC_ERROR_USB_ERROR : SDDC_SUCCESS;
}

sddc_error_t sddc_set_randomizer(sddc_dev_t* dev, bool enabled) {
    // Update the GPIOs according to the desired state
    int err = sddc_gpio_put(dev, SDDC_GPIO_RANDOM, enabled);
    return (err < LIBUSB_SUCCESS) ? SDDC_ERROR_USB_ERROR : SDDC_SUCCESS;
}

sddc_error_t sddc_set_tuner_frequency(sddc_dev_t* dev, uint64_t frequency) {
    // Update the state
    dev->tunerFreq = frequency;

    // If running, send the new frequency to the device
    if (dev->running) {
        int err = sddc_tuner_tune(dev->openDev, frequency);
        if (err < LIBUSB_SUCCESS) { return SDDC_ERROR_USB_ERROR; }
    }

    // Return successfully
    return SDDC_SUCCESS;
}

sddc_error_t sddc_start(sddc_dev_t* dev) {
    // De-assert the shutdown pin
    int err = sddc_gpio_put(dev, SDDC_GPIO_SHUTDOWN, false);
    if (err < LIBUSB_SUCCESS) { return SDDC_ERROR_USB_ERROR; }

    // Start the tuner (TODO: Check if in VHF mode)

    // Start the ADC
    err = sddc_adc_set_samplerate(dev->openDev, dev->samplerate);
    if (err < LIBUSB_SUCCESS) { return SDDC_ERROR_USB_ERROR; }

    // Start the FX3
    err = sddc_fx3_start(dev->openDev);
    if (err < LIBUSB_SUCCESS) { return SDDC_ERROR_USB_ERROR; }

    // Update the state
    dev->running = true;

    // Return successfully
    return SDDC_SUCCESS;
}

sddc_error_t sddc_stop(sddc_dev_t* dev) {
    // Stop the FX3
    int err = sddc_fx3_stop(dev->openDev);
    if (err < LIBUSB_SUCCESS) { return SDDC_ERROR_USB_ERROR; }
    
    // Stop the tuner
    err = sddc_tuner_stop(dev->openDev);
    if (err < LIBUSB_SUCCESS) { return SDDC_ERROR_USB_ERROR; }

    // Stop the ADC
    err = sddc_adc_set_samplerate(dev->openDev, 0);
    if (err < LIBUSB_SUCCESS) { return SDDC_ERROR_USB_ERROR; }

    // Set the GPIOs for standby mode
    err = sddc_gpio_put(dev, SDDC_GPIO_SHUTDOWN, true);
    if (err < LIBUSB_SUCCESS) { return SDDC_ERROR_USB_ERROR; }    

    // Update the state
    dev->running = false;

    // Return successfully
    return SDDC_SUCCESS;
}

sddc_error_t sddc_rx(sddc_dev_t* dev, int16_t* samples, int count) {
    // Read samples from the device
    int bytesRead = 0;
    int err = libusb_bulk_transfer(dev->openDev, LIBUSB_ENDPOINT_IN | 1, samples, count * sizeof(int16_t), &bytesRead, SDDC_TIMEOUT_MS);
    if (err < LIBUSB_SUCCESS) { return SDDC_ERROR_USB_ERROR; }
    return SDDC_SUCCESS;
}