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

// Minimal SHA256 implementation

#include <stdint.h>

typedef struct {
  uint32_t st[8];      // Internal SHA state.
  uint8_t  data[64];   // Remaining data.
  uint64_t bytecnt;    // Total processed bytes.
  int      datasz;     // Remaining data count.
} SHA256_State;

void sha256_init(SHA256_State *state);
void sha256_transform(SHA256_State *state, const void *data, unsigned length);
void sha256_finalize(SHA256_State *state, uint8_t *hash);

void sha256sum(const uint8_t *inbuffer, unsigned length, void *output);
