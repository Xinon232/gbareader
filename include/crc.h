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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

#ifndef _CRC_H_
#define _CRC_H_

// CRC routines
uint8_t crc7(const uint8_t *buf, unsigned size);
uint16_t ds_crc16(const uint8_t *buf, unsigned size);
uint8_t crc7_nolut(const uint8_t *buf, unsigned size);
void crc16_nibble_512(const uint8_t *buf, uint8_t *crcout);
void crc16_nibble_512_nolut(const uint8_t *buf, uint8_t *crcout);
void crc16_nibble_512_nolut8bit(const uint8_t *buf, uint8_t *crcout);
void crc16_nibble_512_nolutw(const uint8_t *buf, uint8_t *crcout);
void crc16_nibble_512_8bit(const uint8_t *buf, uint8_t *crcout);


#endif

