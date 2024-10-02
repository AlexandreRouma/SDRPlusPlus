#include "kcsdr.h"
#include <string.h>
#include "../vendor/FTD3XXLibrary_1.3.0.10/FTD3XX.h"
#include <stdio.h>
#include <stddef.h>

#define KCSDR_PKT_EMPTY_LEN 0x0C

#define KCSDR_COMMAND_PIPE  0x02
#define KCSDR_RX_DATA_PIPE  0x83
#define KCSDR_TX_DATA_PIPE  0x03

struct kcsdr {
    FT_HANDLE ft;
};

#pragma pack(push, 1)
struct kcsdr_packet {
    uint8_t zeros0[4];
    uint8_t length;
    uint8_t zeros1[2];
    uint8_t hex_eighty;
    uint32_t command;
    uint8_t data[188];
};
typedef struct kcsdr_packet kcsdr_packet_t;
#pragma pack(pop)

enum kcsdr_command {
    CMD_NOT_USED_0x00   = 0x00,
    CMD_SET_PORT        = 0x01,
    CMD_SET_FREQUENCY   = 0x02,
    CMD_SET_ATTENUATION = 0x03,
    CMD_SET_AMPLIFIER   = 0x04,
    CMD_SET_BANDWIDTH   = 0x05,
    CMD_START           = 0x06,
    CMD_STOP            = 0x07,
    CMD_SET_EXT_AMP     = 0x08,
    CMD_START_REMOTE    = 0x09,
    CMD_STOP_REMOTE     = 0x0A
};
typedef enum kcsdr_command kcsdr_command_t;

int kcsdr_send_command(kcsdr_t* dev, kcsdr_direction_t dir, kcsdr_command_t cmd, const uint8_t* data, int len) {
    Sleep(50);
    
    // Create an empty packet
    kcsdr_packet_t pkt;
    memset(&pkt, 0, sizeof(kcsdr_packet_t));

    // Fill out the packet info
    pkt.length = len + KCSDR_PKT_EMPTY_LEN;
    pkt.hex_eighty = 0x80; // Whatever the fuck that is
    pkt.command = (uint32_t)cmd | (uint32_t)dir;

    // Copy the data if there is some
    if (len) { memcpy(pkt.data, data, len); }

    // Dump the bytes
    uint8_t* dump = (uint8_t*)&pkt;
    printf("Sending:");
    for (int i = 0; i < pkt.length; i++) {
        printf(" %02X", dump[i]);
    }
    printf("\n");

    // Send the command to endpoint 0
    int sent;
    FT_STATUS err = FT_WritePipeEx(dev->ft, KCSDR_COMMAND_PIPE, (uint8_t*)&pkt, sizeof(kcsdr_packet_t), &sent, NULL);
    if (err != FT_OK) {
        return -err;
    }
    printf("Sent %d bytes (%d)\n", sent, err);

    Sleep(50);

    // Flush existing commands
    FT_FlushPipe(dev->ft, KCSDR_COMMAND_PIPE);

    return -(int)err;
}

int kcsdr_list_devices(kcsdr_info_t** devices) {
    // Generate a list of FTDI devices
    int ftdiDevCount = 0;
    FT_STATUS err = FT_CreateDeviceInfoList(&ftdiDevCount);
    if (err != FT_OK) {
        return -1;
    }

    // If no device was found, return nothing
    if (!ftdiDevCount) {
        *devices = NULL;
        return 0;
    }

    // Get said device list
    FT_DEVICE_LIST_INFO_NODE* list = malloc(ftdiDevCount * sizeof(FT_DEVICE_LIST_INFO_NODE));
    err = FT_GetDeviceInfoList(list, &ftdiDevCount);
    if (err != FT_OK) {
        return -1;
    }

    // Allocate the device info list
    *devices = malloc(ftdiDevCount * sizeof(kcsdr_info_t));

    // Find all KC908s
    int kcCount = 0;
    for (int i = 0; i < ftdiDevCount; i++) {
        strcpy((*devices)[kcCount++].serial, list[i].SerialNumber);
    }

    // Free the FTDI list
    free(list);

    return kcCount;
}

void kcsdr_free_device_list(kcsdr_info_t* devices) {
    // Free the list
    if (devices) { free(devices); }
}

int kcsdr_open(kcsdr_t** dev, const char* serial) {
    // Attempt to open the device using the serial number
    FT_HANDLE ft;
    FT_STATUS err = FT_Create(serial, FT_OPEN_BY_SERIAL_NUMBER, &ft);
    if (err != FT_OK) {
        return -1;
    }

    // Set the timeouts for the data pipes
    FT_SetPipeTimeout(ft, KCSDR_RX_DATA_PIPE, 1000);
    FT_SetPipeTimeout(ft, KCSDR_TX_DATA_PIPE, 1000);

    // Allocate the device object
    *dev = malloc(sizeof(kcsdr_t));

    // Fill out the device object
    (*dev)->ft = ft;

    // Put device into remote control mode
    return kcsdr_send_command(*dev, KCSDR_DIR_RX, CMD_START_REMOTE, NULL, 0);
}

void kcsdr_close(kcsdr_t* dev) {
    // Put device back in normal mode
    kcsdr_send_command(dev, KCSDR_DIR_RX, CMD_STOP_REMOTE, NULL, 0);

    // Close the device
    FT_Close(dev->ft);

    // Free the device object
    free(dev);
}

int kcsdr_set_port(kcsdr_t* dev, kcsdr_direction_t dir, uint8_t port) {
    // Send SET_PORT command
    return kcsdr_send_command(dev, dir, CMD_SET_PORT, &port, 1);
}

int kcsdr_set_frequency(kcsdr_t* dev, kcsdr_direction_t dir, uint64_t freq) {
    // Send SET_FREQUENCY command
    return kcsdr_send_command(dev, dir, CMD_SET_FREQUENCY, (uint8_t*)&freq, 8);
}

int kcsdr_set_attenuation(kcsdr_t* dev, kcsdr_direction_t dir, uint8_t att) {
    // Send SET_ATTENUATION command
    return kcsdr_send_command(dev, dir, CMD_SET_ATTENUATION, &att, 1);
}

int kcsdr_set_amp_gain(kcsdr_t* dev, kcsdr_direction_t dir, uint8_t gain) {
    // Send SET_AMPLIFIER command
    return kcsdr_send_command(dev, dir, CMD_SET_AMPLIFIER, &gain, 1);
}

int kcsdr_set_rx_ext_amp_gain(kcsdr_t* dev, uint8_t gain) {
    // Send CMD_SET_EXT_AMP command
    return kcsdr_send_command(dev, KCSDR_DIR_RX, CMD_SET_EXT_AMP, &gain, 1);
}

int kcsdr_set_samplerate(kcsdr_t* dev, kcsdr_direction_t dir, uint32_t samplerate) {
    // Set SET_BANDWIDTH command
    return kcsdr_send_command(dev, dir, CMD_SET_BANDWIDTH, (uint8_t*)&samplerate, 4);
}

int kcsdr_start(kcsdr_t* dev, kcsdr_direction_t dir) {
    // Send START command
    return kcsdr_send_command(dev, dir, CMD_START, NULL, 0);
}

int kcsdr_stop(kcsdr_t* dev, kcsdr_direction_t dir) {
    // Send STOP command
    return kcsdr_send_command(dev, dir, CMD_STOP, NULL, 0);
}

int kcsdr_rx(kcsdr_t* dev, int16_t* samples, int count) {
    // Receive samples (TODO: Endpoint might be 0x81)
    int received;
    FT_STATUS err = FT_ReadPipeEx(dev->ft, KCSDR_RX_DATA_PIPE, (uint8_t*)samples, count*2*sizeof(uint16_t), &received, NULL);
    return (err == FT_OK) ? received / (2*sizeof(uint16_t)) : -(int)err;
}

int kcsdr_tx(kcsdr_t* dev, const int16_t* samples, int count) {
    // Transmit samples
    int sent;
    FT_STATUS err = FT_WritePipeEx(dev->ft, KCSDR_TX_DATA_PIPE, (uint8_t*)samples, count*2*sizeof(uint16_t), &sent, NULL);
    return (err == FT_OK) ? sent / (2*sizeof(uint16_t)) : -(int)err;
}