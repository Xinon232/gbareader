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

#include "dldi_patcher.h"

#include "util.h"

// Finds a DLDI stub in a buffer and returns an offset (or < 0)
int dldi_stub_find(const void *buffer, unsigned buf_size) {
  if (buf_size < sizeof(t_dldi_header))
    return -1;

  const uint8_t *buf8 = (uint8_t*)buffer;
  for (unsigned i = 0; i < buf_size - sizeof(t_dldi_header); i += 4) {
    const t_dldi_header *h = (t_dldi_header*)&buf8[i];
    if (h->magic == 0xBF8DA5ED &&
        h->signature[0] == 0x69684320 && h->signature[1] == 0x006d6873) {
      if (h->version == 0x1) {
        return i;
      }
    }
  }
  return -1;
}

// Validates a DLDI stub, given a required driver size.
bool dldi_stub_validate(const t_dldi_header *h, unsigned required_size) {
  // Has enough size to patch the driver.
  return ((1 << h->avail_size) >= required_size);
}

// Patches a DLDI driver into a DLDI stub, properly handling flags and relocations.
void dldi_stub_patch(t_dldi_driver *stub, const t_dldi_driver *driver) {
  // Extract info from the driver.
  uint32_t dldi_stub_base = stub->h.addr_start;
  uint8_t avail_size = stub->h.avail_size;

  // The driver might be smaller, extract real size from start/end addrs.
  unsigned driver_size = driver->h.addr_end - driver->h.addr_start;
  memcpy32(stub, driver, driver_size);

  // Restore/Copy some patched fields.
  stub->h.avail_size = avail_size;

  // Clear DLDI's bss area.
  if (driver->h.fix_flags & DLDI_FIX_BSS) {
    unsigned bss_size = driver->h.bss_end - driver->h.bss_start;
    unsigned bss_offs = driver->h.bss_start - driver->h.addr_start;
    uint32_t *bss_ptr = (uint32_t*)(((uint8_t*)stub) + bss_offs);
    for (unsigned i = 0; i < bss_size; i += 4)
      *bss_ptr++ = 0;
  }

  // Proceed to relocate GOT entries.
  if (driver->h.fix_flags & DLDI_FIX_GOT) {
    unsigned got_size = driver->h.got_end - driver->h.got_start;
    unsigned got_offs = driver->h.got_start - driver->h.addr_start;
    uint32_t *got_ptr = (uint32_t*)(((uint8_t*)stub) + got_offs);
    for (unsigned i = 0; i < got_size; i += 4) {
      if (*got_ptr >= driver->h.addr_start && *got_ptr < driver->h.addr_end)
        *got_ptr = *got_ptr - driver->h.addr_start + dldi_stub_base;
      got_ptr++;
    }
  }

  // TODO: Implement GLUE/ALL if ever needed.

  // Patch function pointers.
  uintptr_t off = (uint32_t)((uintptr_t)dldi_stub_base - (uintptr_t)driver->h.addr_start);
  stub->h.startup_func += off;
  stub->h.inserted_func += off;
  stub->h.readsectors_func += off;
  stub->h.writesectors_func += off;
  stub->h.clearstatus_func += off;
  stub->h.shutdown_func += off;

  // Patch also section information
  stub->h.addr_start += off;
  stub->h.addr_end += off;
  stub->h.glue_start += off;
  stub->h.glue_end += off;
  stub->h.got_start += off;
  stub->h.got_end += off;
  stub->h.bss_start += off;
  stub->h.bss_end += off;
}


