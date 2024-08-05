/*
 * Copyright 2013-2016 Benjamin Vernoux <bvernoux@airspy.com>
 *
 * This file is part of AirSpy.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#pragma once
#include <stdint.h>

#define REG_SHADOW_START 5
#define NUM_REGS 30

/* R820T Clock */
#define CALIBRATION_LO 88000

typedef void (*r820t_write_reg_f)(uint8_t reg, uint8_t value, void* ctx);
typedef void (*r820t_read_f)(uint8_t* data, int len, void* ctx);

typedef struct
{
  uint32_t xtal_freq; /* XTAL_FREQ_HZ */
  uint32_t freq;
  uint32_t if_freq;
  uint8_t regs[NUM_REGS];
  uint16_t padding;

  r820t_write_reg_f write_reg;
  r820t_read_f read_reg;
  void* ctx;

} r820t_priv_t;

void airspy_r820t_write_single(r820t_priv_t *priv, uint8_t reg, uint8_t val);
uint8_t airspy_r820t_read_single(r820t_priv_t *priv, uint8_t reg);

int r820t_init(r820t_priv_t *priv, const uint32_t if_freq, r820t_write_reg_f write_reg, r820t_read_f read_reg, void* ctx);
int r820t_set_freq(r820t_priv_t *priv, uint32_t freq);
int r820t_set_lna_gain(r820t_priv_t *priv, uint8_t gain_index);
int r820t_set_mixer_gain(r820t_priv_t *priv, uint8_t gain_index);
int r820t_set_vga_gain(r820t_priv_t *priv, uint8_t gain_index);
int r820t_set_lna_agc(r820t_priv_t *priv, uint8_t value);
int r820t_set_mixer_agc(r820t_priv_t *priv, uint8_t value);
void r820t_set_if_bandwidth(r820t_priv_t *priv, uint8_t bw);