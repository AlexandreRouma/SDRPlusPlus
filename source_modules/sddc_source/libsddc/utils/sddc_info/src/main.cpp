#include <stdio.h>
#include <sddc.h>

int main() {
    // Set firmware image path for debugging
    sddc_set_firmware_path("C:/Users/ryzerth/Downloads/SDDC_FX3 (1).img");

    // List available devices
    sddc_devinfo_t* devList;
    int count = sddc_get_device_list(&devList);
    if (count < 0) {
        fprintf(stderr, "Failed to list devices: %d\n", count);
        return -1;
    }
    else if (!count) {
        printf("No device found.\n");
        return 0;
    }

    // Show the devices in the list
    for (int i = 0; i < count; i++) {
        printf("Serial:   %s\n", devList[i].serial);
        printf("Hardware: %s\n", sddc_model_to_string(devList[i].model));
        printf("Firmware: v%d.%d\n", devList[i].firmwareMajor, devList[i].firmwareMinor);

        // Print separator if needed
        if (i != count-1) {
            printf("\n================================================\n\n");
        }
    }

    // Free the device list and exit
    sddc_free_device_list(devList);
    return 0;
}