#include "fx3_boot.h"
#include <stdio.h>

#define FX3_TIMEOUT         1000
#define FX3_VENDOR_REQUEST  0xA0
#define FX3_MAX_BLOCK_SIZE  0x1000

int sddc_fx3_boot_mem_read(libusb_device_handle* dev, uint32_t addr, uint8_t* data, uint16_t len) {
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, FX3_VENDOR_REQUEST, addr & 0xFFFF, addr >> 16, data, len, FX3_TIMEOUT);
}

int sddc_fx3_boot_mem_write(libusb_device_handle* dev, uint32_t addr, const uint8_t* data, uint16_t len) {
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, FX3_VENDOR_REQUEST, addr & 0xFFFF, addr >> 16, data, len, FX3_TIMEOUT);
}

int sddc_fx3_boot_run(libusb_device_handle* dev, uint32_t entry) {
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, FX3_VENDOR_REQUEST, entry & 0xFFFF, entry >> 16, NULL, 0, FX3_TIMEOUT);
}

int sddc_fx3_boot_upload_firmware(libusb_device_handle* dev, const char* path) {
    // Open the firmware image
    FILE* fw = fopen(path, "rb");
    if (!fw) {
        fprintf(stderr, "Failed to open firmware image\n");
        return LIBUSB_ERROR_OTHER;
    }

    // Read the signature
    char sign[2];
    int read = fread(sign, 2, 1, fw);
    if (read != 1) {
        fprintf(stderr, "Failed to read firmware image signature: %d\n", read);
        fclose(fw);
        return LIBUSB_ERROR_OTHER;
    }

    // Check the signature
    if (sign[0] != 'C' || sign[1] != 'Y') {
        fprintf(stderr, "Firmware image has invalid signature\n");
        fclose(fw);
        return LIBUSB_ERROR_OTHER;
    }

    // Skip useless metadata
    int err = fseek(fw, 2, SEEK_CUR);
    if (err) {
        fprintf(stderr, "Invalid firmware image: %d\n", err);
        fclose(fw);
        return LIBUSB_ERROR_OTHER;
    }

    // Preallocate data buffer
    int bufferSize = 0x10000;
    uint8_t* buffer = malloc(bufferSize);

    // Read every section
    while (1) {
        // Read the section size
        uint32_t sizeWords;
        read = fread(&sizeWords, sizeof(uint32_t), 1, fw);
        if (read != 1) {
            fprintf(stderr, "Invalid firmware image section size\n");
            fclose(fw);
            return LIBUSB_ERROR_OTHER;
        }
        uint32_t size = sizeWords << 2;

        // Read the section address
        uint32_t addr;
        read = fread(&addr, sizeof(uint32_t), 1, fw);
        if (read != 1) {
            fprintf(stderr, "Invalid firmware image section address\n");
            fclose(fw);
            return LIBUSB_ERROR_OTHER;
        }

        // If the section is a termination section, run the code at the given address
        if (!size) {
            sddc_fx3_boot_run(dev, addr);
            break;
        }

        // Re-allocate buffer if needed
        if (size > bufferSize) {
            bufferSize = size;
            realloc(buffer, bufferSize);
        }

        // Read the section data
        read = fread(buffer, 1, size, fw);
        if (read != size) {
            fprintf(stderr, "Failed to read section data: %d\n", read);
            fclose(fw);
            return LIBUSB_ERROR_OTHER;
        }

        // Upload it to the chip
        for (int i = 0; i < size; i += FX3_MAX_BLOCK_SIZE) {
            int left = size - i;
            err = sddc_fx3_boot_mem_write(dev, addr + i, &buffer[i], (left > FX3_MAX_BLOCK_SIZE) ? FX3_MAX_BLOCK_SIZE : left);
            if (err < LIBUSB_SUCCESS) {
                fprintf(stderr, "Failed to write to device memory: %d\n", err);
                fclose(fw);
                return err;
            }
        }
    }

    // TODO: Checksum stuff and verification ideally

    // Close the firmware image
    fclose(fw);

    // Return successfully
    return LIBUSB_SUCCESS;
}