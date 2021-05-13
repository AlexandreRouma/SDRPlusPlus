/*
 * usb_device_internals.h - internal USB structures to be shared between
 *                          usb_device and usb_streaming
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

#ifndef __USB_DEVICE_INTERNALS_H
#define __USB_DEVICE_INTERNALS_H

#include "usb_device.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct usb_device {
  libusb_device *dev;
  libusb_device_handle *dev_handle;
  libusb_context *context;
  int completed;
  int nendpoints;
#define MAX_ENDPOINTS (16)
  struct libusb_endpoint_descriptor endpoints[MAX_ENDPOINTS];
  struct libusb_ss_endpoint_companion_descriptor ss_endpoints[MAX_ENDPOINTS];
  uint8_t bulk_in_endpoint_address;
  uint16_t bulk_in_max_packet_size;
  uint8_t bulk_in_max_burst;
} usb_device_t;
typedef struct usb_device usb_device_t;

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEVICE_INTERNALS_H */
