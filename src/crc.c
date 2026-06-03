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

#include "compiler.h"
#include "crc.h"

#include <stdbool.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  static inline uint32_t read32be(uint32_t v) {
    return (v >> 24) | (v << 24) | ((v >> 8) & 0x0000FF00) | ((v << 8) & 0x00FF0000);
  }
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  static inline uint32_t read32be(uint32_t v) { return v; }
#else
  #error Could not detect platform endianess
#endif

// CRC utility routines for the SD driver.

#define CRC7_POLY      0x09    // x^7 + x^3 + 1

static const uint8_t crc7_lut[256] = {
  0x00, 0x12, 0x24, 0x36, 0x48, 0x5A, 0x6C, 0x7E,
  0x90, 0x82, 0xB4, 0xA6, 0xD8, 0xCA, 0xFC, 0xEE,
  0x32, 0x20, 0x16, 0x04, 0x7A, 0x68, 0x5E, 0x4C,
  0xA2, 0xB0, 0x86, 0x94, 0xEA, 0xF8, 0xCE, 0xDC,
  0x64, 0x76, 0x40, 0x52, 0x2C, 0x3E, 0x08, 0x1A,
  0xF4, 0xE6, 0xD0, 0xC2, 0xBC, 0xAE, 0x98, 0x8A,
  0x56, 0x44, 0x72, 0x60, 0x1E, 0x0C, 0x3A, 0x28,
  0xC6, 0xD4, 0xE2, 0xF0, 0x8E, 0x9C, 0xAA, 0xB8,
  0xC8, 0xDA, 0xEC, 0xFE, 0x80, 0x92, 0xA4, 0xB6,
  0x58, 0x4A, 0x7C, 0x6E, 0x10, 0x02, 0x34, 0x26,
  0xFA, 0xE8, 0xDE, 0xCC, 0xB2, 0xA0, 0x96, 0x84,
  0x6A, 0x78, 0x4E, 0x5C, 0x22, 0x30, 0x06, 0x14,
  0xAC, 0xBE, 0x88, 0x9A, 0xE4, 0xF6, 0xC0, 0xD2,
  0x3C, 0x2E, 0x18, 0x0A, 0x74, 0x66, 0x50, 0x42,
  0x9E, 0x8C, 0xBA, 0xA8, 0xD6, 0xC4, 0xF2, 0xE0,
  0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x70,
  0x82, 0x90, 0xA6, 0xB4, 0xCA, 0xD8, 0xEE, 0xFC,
  0x12, 0x00, 0x36, 0x24, 0x5A, 0x48, 0x7E, 0x6C,
  0xB0, 0xA2, 0x94, 0x86, 0xF8, 0xEA, 0xDC, 0xCE,
  0x20, 0x32, 0x04, 0x16, 0x68, 0x7A, 0x4C, 0x5E,
  0xE6, 0xF4, 0xC2, 0xD0, 0xAE, 0xBC, 0x8A, 0x98,
  0x76, 0x64, 0x52, 0x40, 0x3E, 0x2C, 0x1A, 0x08,
  0xD4, 0xC6, 0xF0, 0xE2, 0x9C, 0x8E, 0xB8, 0xAA,
  0x44, 0x56, 0x60, 0x72, 0x0C, 0x1E, 0x28, 0x3A,
  0x4A, 0x58, 0x6E, 0x7C, 0x02, 0x10, 0x26, 0x34,
  0xDA, 0xC8, 0xFE, 0xEC, 0x92, 0x80, 0xB6, 0xA4,
  0x78, 0x6A, 0x5C, 0x4E, 0x30, 0x22, 0x14, 0x06,
  0xE8, 0xFA, 0xCC, 0xDE, 0xA0, 0xB2, 0x84, 0x96,
  0x2E, 0x3C, 0x0A, 0x18, 0x66, 0x74, 0x42, 0x50,
  0xBE, 0xAC, 0x9A, 0x88, 0xF6, 0xE4, 0xD2, 0xC0,
  0x1C, 0x0E, 0x38, 0x2A, 0x54, 0x46, 0x70, 0x62,
  0x8C, 0x9E, 0xA8, 0xBA, 0xC4, 0xD6, 0xE0, 0xF2,
};

uint8_t crc7(const uint8_t *buf, unsigned size) {
  uint8_t ret = 0;
  while (size--)
    ret = crc7_lut[ret ^ *buf++];
  return ret | 1;
}

// Slower version, not actually used but here for reference.
uint8_t crc7_nolut(const uint8_t *buf, unsigned size) {
  uint8_t ret = 0;  // Initial CRC value, can be adjusted depending on the standard
  for (unsigned i = 0; i < size; i++) {
    ret ^= buf[i];
    for (unsigned j = 0; j < 8; j++) {
      uint8_t pxor = (ret & 0x80) ? CRC7_POLY : 0x00;
      ret = (ret ^ pxor) << 1;
    }
  }
  return ret | 1;
}

static const uint16_t crc16_lut[256] = {
  0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
  0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
  0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
  0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
  0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
  0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
  0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
  0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
  0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,
  0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
  0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
  0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
  0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,
  0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
  0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,
  0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
  0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
  0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
  0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
  0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
  0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
  0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
  0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
  0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
  0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
  0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
  0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
  0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
  0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
  0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
  0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
  0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
};

// This is the regular CRC16 (0x8005 poly) but in reverse (0xA001)
uint16_t ds_crc16(const uint8_t *buf, unsigned size) {
  uint16_t ret = 0xFFFF;
  for (unsigned i = 0; i < size; i++) {
    ret ^= *buf++;
    for (unsigned j = 0; j < 8; j++)
      ret = (ret >> 1) ^ ((ret & 1) ? 0xA001 : 0);
  }
  return ret;
}

// Calculates the CRC16 checksum for the SD card protocol. That is, for each
// of the 4 lanes it calculates a CRC16, then we pack them in nibbles since
// they are sent that way (1x4 bits at a time).
// Data is interleaved and then processed using a LUT for speed.

static inline uint8_t pack_msb_nibbles(uint32_t u) {
  // Picks the MSB for each nibble into a byte.
  // If the word "u" is in the format "abcd.efgh.ijkl.mnop.qrst.uvwx.1234.5678"
  // this returns the byte "15qu.imae" (MSB goes first)
  // Takes advantage of ARM barrel shifter to extract the bits in a clever way.
  uint32_t ph = (u)      & 0x80808080;
  uint32_t pl = (u << 3) & 0x40404040;
  uint32_t m = ph | pl;
  m = (m | (m >> 10));
  m = (m | (m >> 20));
  return (uint8_t)m;
}

// The performance is ~16 cycles per byte, which is roughly 0.5ms per block.

ARM_CODE IWRAM_CODE NOINLINE
void crc16_nibble_512(const uint8_t *buf, uint8_t *crcout) {
  uint16_t d[4] = {0,0,0,0};
  bool aligned = ((((uintptr_t)buf) & 3) == 0);

  if (aligned) {
    for (unsigned i = 0; i < 512 / 4; i++) {
      // Little endian is assumed!
      uint32_t u = *(uint32_t*)buf;
      buf += 4;

      for (unsigned j = 0; j < 4; j++) {
        uint8_t bi = pack_msb_nibbles(u << j);
        uint8_t a = (d[j] >> 8) ^ bi;
        d[j] = (d[j] << 8) ^ crc16_lut[a];
      }
    }
  } else {
    for (unsigned i = 0; i < 512 / 4; i++) {
      // Little endian is assumed!
      uint32_t u = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
      buf += 4;

      for (unsigned j = 0; j < 4; j++) {
        uint8_t bi = pack_msb_nibbles(u << j);
        uint8_t a = (d[j] >> 8) ^ bi;
        d[j] = (d[j] << 8) ^ crc16_lut[a];
      }
    }
  }

  // Store the result sequentially (nibble by nibble) so it can be sent as-is
  // via the Supercard interface.
  for (int i = 7; i >= 0; i--) {
    uint8_t outb = 0;
    uint8_t tbit = 1;
    #pragma GCC unroll 8
    for (int j = 7; j >= 0; j--) {
      if ((d[j & 3]) & 1)
        outb |= tbit;
      d[j & 3] >>= 1;
      tbit <<= 1;
    }
    crcout[i] = outb;
  }
}

// Same as above, but consume input as byte (for SRAM like memory)
ARM_CODE IWRAM_CODE NOINLINE
void crc16_nibble_512_8bit(const uint8_t *buf, uint8_t *crcout) {
  uint16_t d[4] = {0,0,0,0};

  for (unsigned i = 0; i < 512 / 4; i++) {
    // Little endian is assumed!
    uint32_t u = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    buf += 4;

    for (unsigned j = 0; j < 4; j++) {
      uint8_t bi = pack_msb_nibbles(u << j);
      uint8_t a = (d[j] >> 8) ^ bi;
      d[j] = (d[j] << 8) ^ crc16_lut[a];
    }
  }

  // Store the result sequentially (nibble by nibble) so it can be sent as-is
  // via the Supercard interface.
  for (int i = 7; i >= 0; i--) {
    uint8_t outb = 0;
    uint8_t tbit = 1;
    for (int j = 7; j >= 0; j--) {
      if ((d[j & 3]) & 1)
        outb |= tbit;
      d[j & 3] >>= 1;
      tbit <<= 1;
    }
    crcout[i] = outb;
  }
}

/* Uses 64 bit register and interleaves the bits. The poly constant is expanded to 64 bits.
void crc16_nibble_512_nolut_A(const uint8_t *buf, uint8_t *crcout) {
  uint64_t crc = 0;
  for (unsigned i = 0; i < 512; i++) {
    uint64_t lf = (crc >> 60) ^ (buf[i] >> 4);
    crc = (crc << 4);
    for (unsigned j = 0; j < 4; j++)
      if (lf & (1 << j))
        crc ^= 0x0001000000100001ULL << j;

    uint64_t hf = (crc >> 60) ^ (buf[i] & 0xF);
    crc = (crc << 4);
    for (unsigned j = 0; j < 4; j++)
      if (hf & (1 << j))
        crc ^= 0x0001000000100001ULL << j;
  }
  for (int i = 0; i < 8; i++)
    crcout[i] = crc >> ((7-i) * 8);
} */

/*  Same as above, but work one byte at a time.
    Instead of xor-ing with the poly, we iterate poly bits (only 3 bits)
void crc16_nibble_512_nolut(const uint8_t *buf, uint8_t *crcout) {
  uint64_t crc = 0;
  for (unsigned i = 0; i < 512; i++) {
    uint64_t lf = (crc >> 56) ^ buf[i];
    crc = (crc << 8);
    crc ^= lf;
    crc ^= lf << (5*4);
    crc ^= lf << (12*4);
  }
  for (int i = 0; i < 8; i++)
    crcout[i] = crc >> ((7-i) * 8);
} */

// The performance is ~7 cycles per byte, which is roughly 0.25ms per block.

ARM_CODE IWRAM_CODE NOINLINE
void crc16_nibble_512_nolutw(const uint8_t *buf, uint8_t *crcout) {
  uint64_t crc = 0;    // 16 bit per channel, interleaved
  for (unsigned i = 0; i < 512; i += 2) {
    uint64_t lf = (crc >> 48) ^ (buf[i+1] | (buf[i] << 8));
    crc = (crc << 16);
    crc ^= lf;
    crc ^= lf << (5*4);
    crc ^= lf << (12*4);
  }
  for (int i = 0; i < 8; i++)
    crcout[i] = crc >> ((7-i) * 8);
}

// A no-LUT version that works using words. Similar to the half-word version, but
// needs some tweaking, due to overlapping poly bits (doesn't happen with nibbles).
// Has a slower path for non-aligned buffers.
// The performance is ~4.5 cycles per byte, which is roughly 0.1ms per block.

ARM_CODE IWRAM_CODE NOINLINE
void crc16_nibble_512_nolut8bit(const uint8_t *buf, uint8_t *crcout) {
  uint64_t crc = 0;
  for (unsigned i = 0; i < 512; i += 4) {
    uint32_t data32 = buf[i+3] | (buf[i+2] << 8) | (buf[i+1] << 16) | (buf[i] << 24);
    uint64_t lf = (crc >> 32) ^ data32;
    crc = (crc << 32);
    lf ^= lf >> 16;
    crc ^= lf;
    crc ^= lf << (5*4);
    crc ^= lf << (12*4);
  }
  for (int i = 0; i < 8; i++)
    crcout[i] = crc >> ((7-i) * 8);
}

ARM_CODE IWRAM_CODE NOINLINE
void crc16_nibble_512_nolut(const uint8_t *buf, uint8_t *crcout) {
  uint64_t crc = 0;    // 16 bit per channel, interleaved
  bool aligned = ((((uintptr_t)buf) & 3) == 0);

  if (!aligned)
    crc16_nibble_512_nolut8bit(buf, crcout);
  else {
    for (unsigned i = 0; i < 512; i += 4) {
      uint32_t data32 = read32be(*(uint32_t*)&buf[i]);
      uint64_t lf = (crc >> 32) ^ data32;
      crc = (crc << 32);
      lf ^= lf >> 16;        // Propagate overlapping bits
      crc ^= lf;
      crc ^= lf << (5*4);
      crc ^= lf << (12*4);
    }
    for (int i = 0; i < 8; i++)
      crcout[i] = crc >> ((7-i) * 8);
  }
}


