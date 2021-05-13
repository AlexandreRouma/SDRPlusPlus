/*
 * usb_device.c - Basic USB and USB control functions
 *
 * Copyright (C) 2020 by Franco Venturi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* References:
 *  - FX3 SDK for Linux Platforms (https://www.cypress.com/documentation/software-and-drivers/ez-usb-fx3-software-development-kit)
 *    example: cyusb_linux_1.0.5/src/download_fx3.cpp
 */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libusb.h>

#include "usb_device.h"
#include "usb_device_internals.h"
#include "ezusb.h"
#include "logging.h"


typedef struct usb_device usb_device_t;

/* internal functions */
static libusb_device_handle *find_usb_device(int index, libusb_context *ctx,
                             libusb_device **device, int *needs_firmware);
static int load_image(libusb_device_handle *dev_handle,
                      const char *image, uint32_t size);
static int validate_image(const uint8_t *image, const size_t size);
static int transfer_image(const uint8_t *image,
                          libusb_device_handle *dev_handle);
static int list_endpoints(struct libusb_endpoint_descriptor endpoints[],
                          struct libusb_ss_endpoint_companion_descriptor ss_endpoints[],
                          libusb_device *device);


struct usb_device_id {
  uint16_t vid;
  uint16_t pid;
  int needs_firmware;
};


static struct usb_device_id usb_device_ids[] = {
  { 0x04b4, 0x00f3, 1 },     /* Cypress / FX3 Boot-loader */
  { 0x04b4, 0x00f1, 0 }      /* Cypress / FX3 Streamer Example */
};
static int n_usb_device_ids = sizeof(usb_device_ids) / sizeof(usb_device_ids[0]);


int usb_device_count_devices()
{
  int ret_val = -1;

  int ret = libusb_init(0);
  if (ret < 0) {
    log_usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAIL0;
  }
  libusb_device **list = 0;
  ssize_t nusbdevices = libusb_get_device_list(0, &list);
  if (nusbdevices < 0) {
    log_usb_error(nusbdevices, __func__, __FILE__, __LINE__);
    goto FAIL1;
  }
  int count = 0;
  for (ssize_t i = 0; i < nusbdevices; ++i) {
    libusb_device *dev = list[i];
    struct libusb_device_descriptor desc;
    ret = libusb_get_device_descriptor(dev, &desc);
    for (int i = 0; i < n_usb_device_ids; ++i) {
      if (desc.idVendor == usb_device_ids[i].vid &&
          desc.idProduct == usb_device_ids[i].pid) {
        count++;
      }
    }
  }
  libusb_free_device_list(list, 1);

  ret_val = count;

FAIL1:
  libusb_exit(0);
FAIL0:
  return ret_val;
}


int usb_device_get_device_list(struct usb_device_info **usb_device_infos)
{
  const int MAX_STRING_BYTES = 256;

  int ret_val = -1;

  if (usb_device_infos == 0) {
    log_error("argument usb_device_infos is a null pointer", __func__, __FILE__, __LINE__);
    goto FAIL0;
  }

  int ret = libusb_init(0);
  if (ret < 0) {
    log_usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAIL0;
  }
  libusb_device **list = 0;
  ssize_t nusbdevices = libusb_get_device_list(0, &list);
  if (nusbdevices < 0) {
    log_usb_error(nusbdevices, __func__, __FILE__, __LINE__);
    goto FAIL1;
  }

  struct usb_device_info *device_infos = (struct usb_device_info *) malloc((nusbdevices + 1) * sizeof(struct usb_device_info));
  int count = 0;
  for (ssize_t j = 0; j < nusbdevices; ++j) {
    libusb_device *device = list[j];
    struct libusb_device_descriptor desc;
    ret = libusb_get_device_descriptor(device, &desc);
    for (int i = 0; i < n_usb_device_ids; ++i) {
      if (!(desc.idVendor == usb_device_ids[i].vid &&
            desc.idProduct == usb_device_ids[i].pid)) {
        continue;
      }

      libusb_device_handle *dev_handle = 0;
      ret = libusb_open(device, &dev_handle);
      if (ret < 0) {
        log_usb_error(ret, __func__, __FILE__, __LINE__);
        goto FAIL2;
      }

      device_infos[count].manufacturer = (unsigned char *) malloc(MAX_STRING_BYTES);
      device_infos[count].manufacturer[0] = '\0';
      if (desc.iManufacturer) {
        ret = libusb_get_string_descriptor_ascii(dev_handle, desc.iManufacturer,
                      device_infos[count].manufacturer, MAX_STRING_BYTES);
        if (ret < 0) {
          log_usb_error(ret, __func__, __FILE__, __LINE__);
          goto FAIL3;
        }
        device_infos[count].manufacturer = (unsigned char *) realloc(device_infos[count].manufacturer, ret);
      }

      device_infos[count].product = (unsigned char *) malloc(MAX_STRING_BYTES);
      device_infos[count].product[0] = '\0';
      if (desc.iProduct) {
        ret = libusb_get_string_descriptor_ascii(dev_handle, desc.iProduct,
                      device_infos[count].product, MAX_STRING_BYTES);
        if (ret < 0) {
          log_usb_error(ret, __func__, __FILE__, __LINE__);
          goto FAIL3;
        }
        device_infos[count].product = (unsigned char *) realloc(device_infos[count].product, ret);
      }

      device_infos[count].serial_number = (unsigned char *) malloc(MAX_STRING_BYTES);
      device_infos[count].serial_number[0] = '\0';
      if (desc.iSerialNumber) {
        ret = libusb_get_string_descriptor_ascii(dev_handle, desc.iSerialNumber,
                      device_infos[count].serial_number, MAX_STRING_BYTES);
        if (ret < 0) {
          log_usb_error(ret, __func__, __FILE__, __LINE__);
          goto FAIL3;
        }
        device_infos[count].serial_number = (unsigned char *) realloc(device_infos[count].serial_number, ret);
      }

      ret = 0;
FAIL3:
      libusb_close(dev_handle);
      if (ret < 0) {
        goto FAIL2;
      }
      count++;
    }
  }

  device_infos[count].manufacturer = 0;
  device_infos[count].product = 0;
  device_infos[count].serial_number = 0;

  *usb_device_infos = device_infos;
  ret_val = count;

FAIL2:
  libusb_free_device_list(list, 1);
FAIL1:
  libusb_exit(0);
FAIL0:
  return ret_val;
}


int usb_device_free_device_list(struct usb_device_info *usb_device_infos)
{
  for (struct usb_device_info *udi = usb_device_infos;
       udi->manufacturer || udi->product || udi->serial_number;
       ++udi) {
    if (udi->manufacturer) {
      free(udi->manufacturer);
    }
    if (udi->product) {
      free(udi->product);
    }
    if (udi->serial_number) {
      free(udi->serial_number);
    }
  }
  free(usb_device_infos);
  return 0;
}


usb_device_t *usb_device_open(int index, const char* image,
                              uint32_t size)
{
  usb_device_t *ret_val = 0;
  libusb_context *ctx = 0;

  int ret = libusb_init(&ctx);
  if (ret < 0) {
    log_usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAIL0;
  }

  libusb_device *device;
  int needs_firmware = 0;
  libusb_device_handle *dev_handle = find_usb_device(index, ctx, &device, &needs_firmware);
  if (dev_handle == 0) {
    goto FAIL1;
  }

  if (needs_firmware) {
    ret = load_image(dev_handle, image, size);
    if (ret != 0) {
      log_error("load_image() failed", __func__, __FILE__, __LINE__);
      goto FAIL2;
    }

    /* rescan USB to get a new device handle */
    libusb_close(dev_handle);

    /* wait unitl firmware is ready */
    usleep(500 * 1000L);

    needs_firmware = 0;
    dev_handle = find_usb_device(index, ctx, &device, &needs_firmware);
    if (dev_handle == 0) {
      goto FAIL1;
    }
    if (needs_firmware) {
      log_error("device is still in boot loader mode", __func__, __FILE__, __LINE__);
      goto FAIL2;
    }
  }

  int speed = libusb_get_device_speed(device);
  if ( speed == LIBUSB_SPEED_LOW || speed == LIBUSB_SPEED_FULL || speed == LIBUSB_SPEED_HIGH ) {
      log_error("USB 3.x SuperSpeed connection failed", __func__, __FILE__, __LINE__);
      goto FAIL2;
  }

  /* list endpoints */
  struct libusb_endpoint_descriptor endpoints[MAX_ENDPOINTS];
  struct libusb_ss_endpoint_companion_descriptor ss_endpoints[MAX_ENDPOINTS];
  ret = list_endpoints(endpoints, ss_endpoints, device);
  if (ret < 0) {
    log_error("list_endpoints() failed", __func__, __FILE__, __LINE__);
    goto FAIL2;
  }
  int nendpoints = ret;
  uint8_t bulk_in_endpoint_address = 0;
  uint16_t bulk_in_max_packet_size = 0;
  uint8_t bulk_in_max_burst = 0;
  for (int i = 0; i < nendpoints; ++i) {
    if ((endpoints[i].bmAttributes & 0x03) == LIBUSB_TRANSFER_TYPE_BULK &&
        (endpoints[i].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN) {
      bulk_in_endpoint_address = endpoints[i].bEndpointAddress;
      bulk_in_max_packet_size = endpoints[i].wMaxPacketSize;
      bulk_in_max_burst = ss_endpoints[i].bLength == 0 ? 0 :
                          ss_endpoints[i].bMaxBurst;
      break;
    }
  }
  if (bulk_in_endpoint_address == 0) {
    fprintf(stderr, "ERROR - bulk in endpoint not found\n");
    goto FAIL2;
  }

  /* we are good here - create and initialize the usb_device */
  usb_device_t *this = (usb_device_t *) malloc(sizeof(usb_device_t));
  this->dev = device;
  this->dev_handle = dev_handle;
  this->context = ctx;
  this->completed = 0;
  this->nendpoints = nendpoints;
  memset(this->endpoints, 0, sizeof(this->endpoints));
  for (int i = 0; i < nendpoints; ++i) {
    this->endpoints[i] = endpoints[i];
    this->ss_endpoints[i] = ss_endpoints[i];
  }
  this->bulk_in_endpoint_address = bulk_in_endpoint_address;
  this->bulk_in_max_packet_size = bulk_in_max_packet_size;
  this->bulk_in_max_burst = bulk_in_max_burst;

  ret_val = this;
  return ret_val;

FAIL2:
  libusb_close(dev_handle);
FAIL1:
  libusb_exit(0);
FAIL0:
  return ret_val;
}


void usb_device_close(usb_device_t *this)
{
  libusb_close(this->dev_handle);
  free(this);
  libusb_exit(0);
  return;
}


int usb_device_handle_events(usb_device_t *this)
{
  return libusb_handle_events_completed(this->context, &this->completed);
}

int usb_device_control(usb_device_t *this, uint8_t request, uint16_t value,
                       uint16_t index, uint8_t *data, uint16_t length, int read) {

  const uint8_t bmWriteRequestType = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
  const uint8_t bmReadRequestType = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
  const unsigned int timeout = 5000;        // timeout (in ms) for each command
  int ret;

  if (!read) {
      ret = libusb_control_transfer(this->dev_handle, bmWriteRequestType,
                                    request, value, index, data, length,
                                    timeout);
      if (ret < 0) {
        log_usb_error(ret, __func__, __FILE__, __LINE__);
        return -1;
      }
  }
  else
  {
      ret = libusb_control_transfer(this->dev_handle, bmReadRequestType,
                                    request, value, index, data, length,
                                    timeout);
      if (ret < 0) {
        log_usb_error(ret, __func__, __FILE__, __LINE__);
        return -1;
      }
  }

  return 0;
}



/* internal functions */
static libusb_device_handle *find_usb_device(int index, libusb_context *ctx,
                             libusb_device **device, int *needs_firmware)
{
  libusb_device_handle *ret_val = 0;

  *device = 0;
  *needs_firmware = 0;

  libusb_device **list = 0;
  ssize_t nusbdevices = libusb_get_device_list(ctx, &list);
  if (nusbdevices < 0) {
    log_usb_error(nusbdevices, __func__, __FILE__, __LINE__);
    goto FAIL0;
  }

  int count = 0;
  for (ssize_t j = 0; j < nusbdevices; ++j) {
    libusb_device *dev = list[j];
    struct libusb_device_descriptor desc;
    libusb_get_device_descriptor(dev, &desc);
    for (int i = 0; i < n_usb_device_ids; ++i) {
      if (desc.idVendor == usb_device_ids[i].vid &&
          desc.idProduct == usb_device_ids[i].pid) {
        if (count == index) {
          *device = dev;
          *needs_firmware = usb_device_ids[i].needs_firmware;
        }
        count++;
      }
    }
  }

  if (*device == 0) {
    fprintf(stderr, "ERROR - usb_device@%d not found\n", index);
    goto FAIL1;
  }

  libusb_device_handle *dev_handle = 0;
  int ret = libusb_open(*device, &dev_handle);
  if (ret < 0) {
    log_usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAIL1;
  }
  libusb_free_device_list(list, 1);

  ret = libusb_kernel_driver_active(dev_handle, 0);
  if (ret < 0) {
    log_usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAILA;
  }
  if (ret == 1) {
    fprintf(stderr, "ERROR - device busy\n");
    goto FAILA;
  }

  ret = libusb_claim_interface(dev_handle, 0);
  if (ret < 0) {
    log_usb_error(ret, __func__, __FILE__, __LINE__);
    goto FAILA;
  }

  ret_val = dev_handle;
  return ret_val;

FAILA:
  libusb_close(dev_handle);
  return ret_val;

FAIL1:
  libusb_free_device_list(list, 1);
FAIL0:
  return ret_val;
}


int load_image(libusb_device_handle *dev_handle, const char *image, uint32_t image_size)
{
  int ret_val = -1;

  const int fx_type = FX_TYPE_FX3;
  const int img_type = IMG_TYPE_IMG;
  const int stage = 0;
  verbose = 1;

  ret_val = fx3_load_ram(dev_handle, image);
  return ret_val;
}


static int transfer_image(const uint8_t *image,
                          libusb_device_handle *dev_handle)
{
  const uint8_t bmRequestType = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE;
  const uint8_t bRequest = 0xa0;            // vendor command
  const unsigned int timeout = 5000;        // timeout (in ms) for each command
  const size_t max_write_size = 2 * 1024;   // max write size in bytes
 
  // skip first word with 'CY' magic
  uint32_t *current = (uint32_t *) image + 1;

  while (1) {
    uint32_t loadSz = *current++;
    if (loadSz == 0) {
      break;
    }
    uint32_t address = *current++;

    unsigned char *data = (unsigned char *) current;
    for (size_t nleft = loadSz * 4; nleft > 0; ) {
      uint16_t wLength = nleft > max_write_size ? max_write_size : nleft;
      int ret = libusb_control_transfer(dev_handle, bmRequestType, bRequest,
                                        address & 0xffff, address >> 16,
                                        data, wLength, timeout);
      if (ret < 0) {
        log_usb_error(ret, __func__, __FILE__, __LINE__);
        return -1;
      }
      if (!(ret == wLength)) {
        fprintf(stderr, "ERROR - libusb_control_transfer() returned less bytes than expected - actual=%hu expected=%hu\n", ret, wLength);
        return -1;
      }
      data += wLength;
      nleft -= wLength;
    }
    current += loadSz;
  }

  uint32_t entryAddr = *current++;
  uint32_t checksum __attribute__((unused)) = *current++;

  sleep(1);

  int ret = libusb_control_transfer(dev_handle, bmRequestType, bRequest,
                                    entryAddr & 0xffff, entryAddr >> 16,
                                    0, 0, timeout);
  if (ret < 0) {
    log_usb_warning(ret, __func__, __FILE__, __LINE__);
  }

  return 0;
}


static int list_endpoints(struct libusb_endpoint_descriptor endpoints[],
                          struct libusb_ss_endpoint_companion_descriptor ss_endpoints[],
                          libusb_device *device)
{
  struct libusb_config_descriptor *config;
  int ret = libusb_get_active_config_descriptor(device, &config);
  if (ret < 0) {
    log_usb_error(ret, __func__, __FILE__, __LINE__);
    return -1;
  }

  int count = 0;

  /* loop through the interfaces */
  for (int intf = 0; intf < config->bNumInterfaces; ++intf) {
    const struct libusb_interface *interface = &config->interface[intf];
    for (int setng = 0; setng < interface->num_altsetting; ++setng) {
      const struct libusb_interface_descriptor *setting = &interface->altsetting[setng];
      for (int endp = 0; endp < setting->bNumEndpoints; ++endp) {
        const struct libusb_endpoint_descriptor *endpoint = &setting->endpoint[endp];
        if (count == MAX_ENDPOINTS) {
          fprintf(stderr, "WARNING - found too many USB endpoints; returning only the first %d\n", MAX_ENDPOINTS);
          return count;
        }
        endpoints[count] = *endpoint;
        struct libusb_ss_endpoint_companion_descriptor *endpoint_ss_companion;
        ret = libusb_get_ss_endpoint_companion_descriptor(0, endpoint,
                &endpoint_ss_companion);
        if (ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND) {
          log_usb_error(ret, __func__, __FILE__, __LINE__);
          return -1;
        }
        if (ret == 0) {
          ss_endpoints[count] = *endpoint_ss_companion;
        } else {
          ss_endpoints[count].bLength = 0;
        }
        libusb_free_ss_endpoint_companion_descriptor(endpoint_ss_companion);
        count++;
      }
    }
  }

  return count;
}
