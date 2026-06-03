/*
 * Copyright (C) 2024 David Guillen Fandos <david@davidgf.net>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

// SuperFW's DLDI driver for Supercard.
// This is used in NDS mode, but can also be used in several other modes
// such as Direct-Saving to write .sav files to SD directly.

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "supercard_driver.h"

#define REG_EXMEMCNT     (*((volatile uint16_t*)0x04000204))

static void supercard_prepare(bool enable_sd) {
  REG_EXMEMCNT &= ~0x80;    // ARM9 has access rights, does nothing on ARM7.
  // Enable/Disable SD card interface.
  set_supercard_mode(MAPPED_SDRAM, true, enable_sd);
}

bool dldi_startup() {
  supercard_prepare(true);

  // Initialize the SD card from scratch.
  unsigned errc = sdcard_init(NULL);

  supercard_prepare(false);
  return !errc;
}

bool dldi_inserted() {
  return true;
}

bool dldi_readsectors(uint32_t sector, uint32_t num_sectors, uint8_t *buffer) {
  supercard_prepare(true);

  unsigned errc = sdcard_read_blocks(buffer, sector, num_sectors);

  supercard_prepare(false);
  return !errc;
}

bool dldi_writesectors(uint32_t sector, uint32_t num_sectors, const void *buffer) {
  supercard_prepare(true);

  unsigned errc = sdcard_write_blocks(buffer, sector, num_sectors);

  supercard_prepare(false);
  return !errc;
}

bool dldi_clearstatus() {
  return true;
}

bool dldi_shutdown() {
  return true;
}

