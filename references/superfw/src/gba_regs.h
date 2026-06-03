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


#ifndef GBA_REGS_H
#define GBA_REGS_H

// Video/Display registers
#define REG_DISPCNT   0x000
#define REG_DISPSTAT  0x004
#define REG_VCOUNT    0x006
#define REG_BG0CNT    0x008
#define REG_BG1CNT    0x00A
#define REG_BG2CNT    0x00C
#define REG_BG3CNT    0x00E
#define REG_BG0HOFS   0x010
#define REG_BG0VOFS   0x012
#define REG_BG1HOFS   0x014
#define REG_BG1VOFS   0x016
#define REG_BG2HOFS   0x018
#define REG_BG2VOFS   0x01A
#define REG_BG3HOFS   0x01C
#define REG_BG3VOFS   0x01E
#define REG_BG2PA     0x020
#define REG_BG2PB     0x022
#define REG_BG2PC     0x024
#define REG_BG2PD     0x026
#define REG_BG2X      0x028
#define REG_BG2Y      0x02C
#define REG_BG3PA     0x030
#define REG_BG3PB     0x032
#define REG_BG3PC     0x034
#define REG_BG3PD     0x036
#define REG_BG3X      0x038
#define REG_BG3Y      0x03C
#define REG_WIN0H     0x040
#define REG_WIN1H     0x042
#define REG_WIN0V     0x044
#define REG_WIN1V     0x046
#define REG_WININ     0x048
#define REG_WINOUT    0x04A
#define REG_MOSAIC    0x04C
#define REG_BLDCNT    0x050
#define REG_BLDALPHA  0x052
#define REG_BLDY       0x054

// Sound control registers
#define REG_SOUND1CNT_L 0x060
#define REG_SOUND1CNT_H 0x062
#define REG_SOUND1CNT_X 0x064
#define REG_SOUND2CNT_L 0x068
#define REG_SOUND2CNT_H 0x06C
#define REG_SOUND3CNT_L 0x070
#define REG_SOUND3CNT_H 0x072
#define REG_SOUND3CNT_X 0x074
#define REG_SOUND4CNT_L 0x078
#define REG_SOUND4CNT_H 0x07C
#define REG_SOUNDCNT_L  0x080
#define REG_SOUNDCNT_H  0x082
#define REG_SOUNDCNT_X  0x084
#define REG_SOUNDWAVE   0x090
#define REG_SOUND_FIFOA 0x0A0
#define REG_SOUND_FIFOB 0x0A4

// DMA control
#define REG_DMA0SAD    0x0B0
#define REG_DMA0DAD    0x0B4
#define REG_DMA0CNT_L  0x0B8
#define REG_DMA0CNT_H  0x0BA
#define REG_DMA1SAD    0x0BC
#define REG_DMA1DAD    0x0C0
#define REG_DMA1CNT_L  0x0C4
#define REG_DMA1CNT_H  0x0C6
#define REG_DMA2SAD    0x0C8
#define REG_DMA2DAD    0x0CC
#define REG_DMA2CNT_L  0x0D0
#define REG_DMA2CNT_H  0x0D2
#define REG_DMA3SAD    0x0D4
#define REG_DMA3DAD    0x0D8
#define REG_DMA3CNT_L  0x0DC
#define REG_DMA3CNT_H  0x0DE

// Timers
#define REG_TM0D   0x100
#define REG_TM0CNT 0x102
#define REG_TM1D   0x104
#define REG_TM1CNT 0x106
#define REG_TM2D   0x108
#define REG_TM2CNT 0x10A
#define REG_TM3D   0x10C
#define REG_TM3CNT 0x10E

// Key input
#define REG_P1    0x130
#define REG_P1CNT 0x132

// Interrupt, waitstate, power.
#define REG_IE      0x200
#define REG_IF      0x202
#define REG_WAITCNT 0x204
#define REG_IME     0x208
#define REG_HALTCNT 0x300  // PostFLG + HaltCNT

#endif

