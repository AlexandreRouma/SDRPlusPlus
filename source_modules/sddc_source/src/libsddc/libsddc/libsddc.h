/*
 * libsddc - low level functions for wideband SDR receivers like
 *           BBRF103, RX-666, RX888, HF103, etc
 *
 * Copyright (C) 2020 by Franco Venturi
 *
 * this program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __LIBSDDC_H
#define __LIBSDDC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct sddc sddc_t;

struct sddc_device_info {
  const char *manufacturer;
  const char *product;
  const char *serial_number;
};

enum SDDCStatus {
  SDDC_STATUS_OFF,
  SDDC_STATUS_READY,
  SDDC_STATUS_STREAMING,
  SDDC_STATUS_FAILED = 0xff
};

enum SDDCHWModel {
  HW_NORADIO,
  HW_BBRF103,
  HW_HF103,
  HW_RX888,
  HW_RX888R2,
  HW_RX999,
  HW_RX888R3,
};

enum RFMode {
  NO_RF_MODE,
  HF_MODE,
  VHF_MODE
};

enum LEDColors {
  YELLOW_LED = 0x01,
  RED_LED    = 0x02,
  BLUE_LED   = 0x04
};

/* basic functions */
int sddc_get_device_count();

int sddc_get_device_info(struct sddc_device_info **sddc_device_infos);

int sddc_free_device_info(struct sddc_device_info *sddc_device_infos);

sddc_t *sddc_open(int index, const char* imagefile);

void sddc_close(sddc_t *t);

enum SDDCStatus sddc_get_status(sddc_t *t);

enum SDDCHWModel sddc_get_hw_model(sddc_t *t);

const char *sddc_get_hw_model_name(sddc_t *t);

uint16_t sddc_get_firmware(sddc_t *t);

const double *sddc_get_frequency_range(sddc_t *t);

enum RFMode sddc_get_rf_mode(sddc_t *t);

int sddc_set_rf_mode(sddc_t *t, enum RFMode rf_mode);


/* LED functions */
int sddc_led_on(sddc_t *t, uint8_t led_pattern);

int sddc_led_off(sddc_t *t, uint8_t led_pattern);

int sddc_led_toggle(sddc_t *t, uint8_t led_pattern);


/* ADC functions */
int sddc_get_adc_dither(sddc_t *t);

int sddc_set_adc_dither(sddc_t *t, int dither);

int sddc_get_adc_random(sddc_t *t);

int sddc_set_adc_random(sddc_t *t, int random);


/* HF block functions */
double sddc_get_hf_attenuation(sddc_t *t);

int sddc_set_hf_attenuation(sddc_t *t, double attenuation);

int sddc_get_hf_bias(sddc_t *t);

int sddc_set_hf_bias(sddc_t *t, int bias);


/* VHF block and VHF/UHF tuner functions */
double sddc_get_tuner_frequency(sddc_t *t);

int sddc_set_tuner_frequency(sddc_t *t, double frequency);

int sddc_get_tuner_rf_attenuations(sddc_t *t, const double *attenuations[]);

double sddc_get_tuner_rf_attenuation(sddc_t *t);

int sddc_set_tuner_rf_attenuation(sddc_t *t, double attenuation);

int sddc_get_tuner_if_attenuations(sddc_t *t, const double *attenuations[]);

double sddc_get_tuner_if_attenuation(sddc_t *t);

int sddc_set_tuner_if_attenuation(sddc_t *t, double attenuation);

int sddc_get_vhf_bias(sddc_t *t);

int sddc_set_vhf_bias(sddc_t *t, int bias);


/* streaming functions */
typedef void (*sddc_read_async_cb_t)(uint32_t data_size, uint8_t *data,
                                      void *context);

double sddc_get_sample_rate(sddc_t *t);

int sddc_set_sample_rate(sddc_t *t, double sample_rate);

int sddc_set_async_params(sddc_t *t, uint32_t frame_size, 
                          uint32_t num_frames, sddc_read_async_cb_t callback,
                          void *callback_context);

int sddc_start_streaming(sddc_t *t);

int sddc_handle_events(sddc_t *t);

int sddc_stop_streaming(sddc_t *t);

int sddc_reset_status(sddc_t *t);

int sddc_read_sync(sddc_t *t, uint8_t *data, int length, int *transferred);

#ifdef __cplusplus
}
#endif

#endif /* __LIBSDDC_H */