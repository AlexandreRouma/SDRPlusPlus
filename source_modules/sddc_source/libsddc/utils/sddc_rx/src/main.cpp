#include <stdio.h>
#include <sddc.h>

int main() {
    // Set firmware image path for debugging
    sddc_set_firmware_path("C:/Users/ryzerth/Downloads/SDDC_FX3 (1).img");

    // Open the device
    sddc_dev_t* dev;
    sddc_error_t err = sddc_open("0009072C00C40C32", &dev);
    if (err != SDDC_SUCCESS) {
        printf("Failed to open device: %s (%d)\n", sddc_error_to_string(err), err);
        return -1;
    }

    // Configure the device
    sddc_set_samplerate(dev, 8e6);

    // Start the device
    sddc_start(dev);

    // Continuous read samples
    const int bufSize = 1e6;
    int16_t* buffer = new int16_t[bufSize];
    while (true) {
        // Read
        sddc_error_t err = sddc_rx(dev, buffer, bufSize);
        printf("Samples received: %d\n", err);
    }

    // Stop the device
    sddc_stop(dev);

    // Close the device
    sddc_close(dev);

    return 0;
}