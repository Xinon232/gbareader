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

#ifndef _DLDI_PATCHER_H_
#define _DLDI_PATCHER_H_

#include <stdint.h>
#include <stdbool.h>

#define DLDI_FIX_ALL    0x1
#define DLDI_FIX_GLUE   0x2
#define DLDI_FIX_GOT    0x4
#define DLDI_FIX_BSS    0x8

// This is the usual DLDI header, with some custom extras that we re-use

typedef struct {
  uint32_t magic;
  uint32_t signature[2];   // " Chishm"

  uint8_t  version;
  uint8_t  req_size;
  uint8_t  fix_flags;
  uint8_t  avail_size;

  uint8_t  driver_name[48];

  uint32_t addr_start;
  uint32_t addr_end;
  uint32_t glue_start;
  uint32_t glue_end;
  uint32_t got_start;
  uint32_t got_end;
  uint32_t bss_start;
  uint32_t bss_end;

  uint32_t iotype;         // 4 ascii chars
  uint32_t feature_flags;

  uint32_t startup_func;
  uint32_t inserted_func;
  uint32_t readsectors_func;
  uint32_t writesectors_func;
  uint32_t clearstatus_func;
  uint32_t shutdown_func;
} t_dldi_header;

typedef struct {
  t_dldi_header h;

  uint8_t data[];
} t_dldi_driver;


// Finds a DLDI stub in a buffer and returns an offset (or < 0)
int dldi_stub_find(const void *buffer, unsigned buf_size);

// Validates a DLDI stub, given a required driver size.
bool dldi_stub_validate(const t_dldi_header *h, unsigned required_size);

// Patches a DLDI driver into a DLDI stub, properly handling flags and relocations.
void dldi_stub_patch(t_dldi_driver *stub, const t_dldi_driver *driver);

#endif

