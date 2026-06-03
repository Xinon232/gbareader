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

#include <stdio.h>
#include <stdlib.h>
#include "dldi_patcher.h"

// Test tool that patches NDS files with a DLDI driver.
// It clears out the patched DLDI stub header to avoid double patching.

void memcpy32(void *restrict dst, const void *restrict src, unsigned count) {
  uint32_t *dst32 = (uint32_t*)dst;
  uint32_t *src32 = (uint32_t*)src;
  for (unsigned i = 0; i < count; i+=4)
    *dst32++ = *src32++;
}

void *readfile(const char *fn, unsigned *fs) {
  FILE *fd = fopen(fn, "rb");
  if (!fd)
    return NULL;
  const unsigned max_size = 4*1024*1024;
  char *buf = malloc(max_size);
  int ret = fread(buf, 1, max_size, fd);
  if (ret < 0 || ret >= max_size) {
    free(buf);
    return NULL;
  }

  if (fs)
    *fs = ret;
  return buf;
}

int main(int argc, char **argv) {
  if (argc < 4) {
    printf("Usage: %s input.nds driver.dldi output.nds\n", argv[0]);
    exit(1);
  }

  unsigned nds_fs, dldi_fs;
  char *nds = readfile(argv[1], &nds_fs);
  if (!nds) {
    printf("Cannot open and read file %s\n", argv[1]);
    exit(1);  
  }

  char *drv = readfile(argv[2], &dldi_fs);
  if (!drv) {
    printf("Cannot open and read file %s\n", argv[2]);
    exit(1);
  }

  // Need to parse the NDS a bit: only patch arm9/arm7 areas!
  int offset = 0;
  while (offset < nds_fs) {
    int next_offset = dldi_stub_find(&nds[offset], nds_fs - offset);
    if (next_offset < 0)
      break;
    offset += next_offset;

    t_dldi_header *dldi_stub = (t_dldi_header*)&nds[offset];
    if (dldi_stub_validate(dldi_stub, dldi_fs)) {
      printf("Patching DLDI at offset %d\n", offset);
      dldi_stub_patch((t_dldi_driver*)dldi_stub, (t_dldi_driver*)drv);
    }
    // Clear the header as well, after patching
    dldi_stub->magic = 0;
    dldi_stub->signature[0] = 0;
    dldi_stub->signature[1] = 0;

    offset += 4;
  }

  FILE *fdw = fopen(argv[3], "wb");
  if (!fdw) {
    printf("Could not open %s for writing!\n", argv[3]);
    exit(1);
  }

  fwrite(nds, 1, nds_fs, fdw);
  fclose(fdw);
}

