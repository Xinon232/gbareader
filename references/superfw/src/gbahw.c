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

#include <stdint.h>

#include "gbahw.h"

void dma_memset16(volatile void *ptr, uint16_t value, uint16_t count) {
  if (!count)
    return;        // Zero is not a valid count!
  volatile uint16_t dmaval = value;
  DMA_SAD(3) = (uint32_t)&dmaval;
  DMA_DAD(3) = (uint32_t)ptr;
  DMA_CTL(3) = ((DMA_DST_INC | DMA_SRC_FIXED | DMA_ENABLE) << 16) | count;
  asm volatile("": : :"memory");
}

void dma_memcpy16(volatile void *dst, const void *src, uint16_t count) {
  DMA_SAD(3) = (uint32_t)src;
  DMA_DAD(3) = (uint32_t)dst;
  DMA_CTL(3) = ((DMA_DST_INC | DMA_SRC_INC | DMA_ENABLE) << 16) | count;
  asm volatile("": : :"memory");
}

void dma_memcpy32(volatile void *dst, const void *src, uint16_t count) {
  DMA_SAD(3) = (uint32_t)src;
  DMA_DAD(3) = (uint32_t)dst;
  DMA_CTL(3) = ((DMA_TRANSFER32 | DMA_DST_INC | DMA_SRC_INC | DMA_ENABLE) << 16) | count;
  asm volatile("": : :"memory");
}

