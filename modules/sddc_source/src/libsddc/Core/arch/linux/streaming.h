/*
 * streaming.h - streaming functions
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

#ifndef __STREAMING_H
#define __STREAMING_H

#include "usb_device.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct streaming streaming_t;

typedef void (*sddc_read_async_cb_t)(uint32_t data_size, uint8_t *data,
		                                      void *context);

streaming_t *streaming_open_sync(usb_device_t *usb_device);

streaming_t *streaming_open_async(usb_device_t *usb_device, uint32_t frame_size,
                                  uint32_t num_frames,
                                  sddc_read_async_cb_t callback,
                                  void *callback_context);

void streaming_close(streaming_t *that);

int streaming_set_sample_rate(streaming_t *that, uint32_t sample_rate);

int streaming_set_random(streaming_t *that, int random);

int streaming_start(streaming_t *that);

int streaming_stop(streaming_t *that);

int streaming_reset_status(streaming_t *that);

int streaming_read_sync(streaming_t *that, uint8_t *data, int length,
                        int *transferred);

#ifdef __cplusplus
}
#endif

#endif /* __STREAMING_H */
