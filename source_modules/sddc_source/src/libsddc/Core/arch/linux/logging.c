/*
 * logging.c - logging functions
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

#include <stdio.h>
#include <libusb.h>

#include "logging.h"


void log_error(const char *error_message, const char *function,
               const char *file, int line) {
  fprintf(stderr, "ERROR - %s in %s at %s:%d\n", error_message, function,
          file, line);
  return;
}

void log_usb_error(int usb_error_code, const char *function, const char *file,
                   int line) {
  fprintf(stderr, "ERROR - USB error %s in %s at %s:%d\n",
          libusb_error_name(usb_error_code), function, file, line);
  return;
}

void log_usb_warning(int usb_error_code, const char *function, const char *file,
                     int line) {
  fprintf(stderr, "WARNING - USB warning %s in %s at %s:%d\n",
          libusb_error_name(usb_error_code), function, file, line);
  return;
}
