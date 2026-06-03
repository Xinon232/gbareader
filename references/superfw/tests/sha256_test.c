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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <openssl/sha.h>

#include "sha256.h"

int main() {
  const struct {
    const char *data;
    const uint8_t len;
    const char *hash;
  } testvec[] = {
    {
      "", 0,
      "\xe3\xb0\xc4\x42\x98\xfc\x1c\x14\x9a\xfb\xf4\xc8\x99\x6f\xb9\x24"
      "\x27\xae\x41\xe4\x64\x9b\x93\x4c\xa4\x95\x99\x1b\x78\x52\xb8\x55",
    },
    {
      "test", 4,
      "\x9f\x86\xd0\x81\x88\x4c\x7d\x65\x9a\x2f\xea\xa0\xc5\x5a\xd0\x15"
      "\xa3\xbf\x4f\x1b\x2b\x0b\x82\x2c\xd1\x5d\x6c\x15\xb0\xf0\x0a\x08",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ012", 55,
      "\xd7\x4b\xa0\x75\xe4\x25\x9c\x6c\x80\x7c\x41\x01\xe6\x6d\x28\x10"
      "\x96\xcf\x9f\xf1\x4b\xa0\x12\x60\xde\xe7\x41\xb1\xbd\xae\xf3\x26",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123", 56,
      "\x8f\xb6\x05\xea\xb2\xef\xae\x3d\x1f\xcc\x88\x1f\xa5\xc5\xdd\x62"
      "\x19\xa1\x7c\xa3\x66\x3e\x46\x64\x2f\xf5\x66\x84\x7c\x24\xc2\x72",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234", 57,
      "\xf6\x57\x70\x0b\xee\x98\xbf\x60\x88\x04\x01\xa6\xea\x1e\x6e\x32"
      "\xfe\xcc\x61\xcf\x4e\x22\xda\xb5\x60\xf5\x8a\xd3\x0e\x00\x14\x82",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ012345", 58,
      "\xbf\x1b\x8a\xf8\x13\x0a\x85\x49\xa0\xb2\x63\x32\x67\x8e\x53\x2f"
      "\x46\xf9\x89\xd2\x9c\x61\xcb\xd3\x98\xc3\xfd\x9b\x62\xe6\x44\x8e",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456", 59,
      "\x60\xd0\xba\x2d\x35\x10\xc2\x43\xf1\xb6\x19\xda\xc3\x82\xd6\xa7"
      "\xde\xe5\x0e\xb0\x2f\x87\x1e\x59\xc1\x06\x6f\x72\x8c\x7b\xd8\x02",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567", 60,
      "\xa5\xc1\x35\x4c\x0c\xcb\x75\x3a\x33\xba\x69\x78\xbf\x25\x0f\xd8"
      "\xd2\x53\x05\x64\x81\xef\xe7\x4e\x96\x61\x98\x0a\xe1\x76\x67\x51",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ012345678", 61,
      "\xa1\x17\x59\x08\x61\x8a\x33\xd8\x78\x3d\xa0\x18\x6c\x70\x88\x87"
      "\x8b\xa8\xed\xb9\x5a\xee\x2f\xf6\xa3\x16\x5d\x99\xe8\x0e\x16\xa5",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 62,
      "\x54\x03\x63\xd1\x07\x1a\x00\x29\x97\x29\x0c\xd8\xf4\xa2\xbd\xf3"
      "\xac\xd0\x35\x5f\xfa\xd3\xb3\xf2\x5f\x52\xaa\xd6\xeb\xad\x93\x6a",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", 63,
      "\x74\x33\xed\xe2\x56\x5c\xd5\x35\x06\x35\x8f\x27\x95\xc3\x23\x7c"
      "\x71\x47\x85\xda\xcd\xca\x6e\x8f\x4f\xb7\xb1\x6f\xe7\x8e\xe1\xec",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-", 64,
      "\x58\x3f\x4c\x14\xd7\x1c\xea\x26\x19\xf1\xe8\x0f\x07\xc0\x3b\xee"
      "\x6f\x85\xfd\x2e\x2e\x26\x40\x82\xc8\x45\x91\x44\x1b\x01\x41\x1c",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.", 65,
      "\x17\x40\xc1\x34\xff\x16\xa7\x11\xf1\xaf\x62\x1e\x14\x80\x38\xce"
      "\x75\x4d\x89\x6b\x21\xe2\x7a\xcd\xe2\x4c\x42\x01\x6c\x77\x45\x6d",
    },
    {
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.,", 66,
      "\x9b\xcc\x60\xc3\x1d\xd9\x7e\x84\x0e\x81\x2e\xa7\x88\x60\xa5\x07"
      "\x17\x8d\xe3\x98\x3d\x0a\xd1\x2f\x69\xa7\x68\x24\x1f\xe0\xeb\xdc",
    },
  };
  
  for (unsigned i = 0; i < sizeof(testvec)/sizeof(testvec[0]); i++) {
    uint8_t h[32];
    sha256sum((uint8_t*)testvec[i].data, testvec[i].len, h);
    assert(!memcmp(h, testvec[i].hash, sizeof(h)));
  }

  for (unsigned i = 0; i < sizeof(testvec)/sizeof(testvec[0]); i++) {
    uint8_t h[32];
    SHA256_State st;
    sha256_init(&st);
    for (unsigned j = 0; j < testvec[i].len; j++)
      sha256_transform(&st, &testvec[i].data[j], 1);

    sha256_finalize(&st, h);
    assert(!memcmp(h, testvec[i].hash, sizeof(h)));
  }

  // Try some random vectors:
  for (unsigned i = 0; i < 256*1024; i++) {
    uint8_t href[32], h[32];
    uint8_t tmp[256];
    unsigned size = rand() % 256;
    for (unsigned j = 0; j < size; j++)
      tmp[j] = rand();

    SHA256(tmp, size, href);
    sha256sum(tmp, size, h);
    assert(!memcmp(h, href, sizeof(h)));
  }
}


