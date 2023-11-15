/*--------------------------------------------------------------------------
*  File name:  vga.h
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 14-11-2022
*--------------------------------------------------------------------------
Rotinas para manipulação de vídeo svga
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "io.h"
#include "ktypes.h"

#ifndef _VGA_H_
#define _VGA_H_

/* --------------------- Macro definitions shared by library modules */

/* VGA index register ports */
#define VGA_CRT_IC 0x3D4 /* CRT Controller Index - color emulation */
#define VGA_CRT_IM 0x3B4 /* CRT Controller Index - mono emulation */
#define VGA_ATT_IW 0x3C0 /* Attribute Controller Index & Data Write Register */
#define VGA_GRA_I 0x3CE  /* Graphics Controller Index */
#define VGA_SEQ_I 0x3C4  /* Sequencer Index */
#define VGA_PEL_IW 0x3C8 /* PEL Write Index */
#define VGA_PEL_IR 0x3C7 /* PEL Read Index */

/* VGA data register ports */
#define VGA_CRT_DC 0x3D5  /* CRT Controller Data Register - color emulation */
#define VGA_CRT_DM 0x3B5  /* CRT Controller Data Register - mono emulation */
#define VGA_ATT_R 0x3C1   /* Attribute Controller Data Read Register */
#define VGA_GRA_D 0x3CF   /* Graphics Controller Data Register */
#define VGA_SEQ_D 0x3C5   /* Sequencer Data Register */
#define VGA_MIS_R 0x3CC   /* Misc Output Read Register */
#define VGA_MIS_W 0x3C2   /* Misc Output Write Register */
#define VGA_IS1_RC 0x3DA  /* Input Status Register 1 - color emulation */
#define VGA_IS1_RM 0x3BA  /* Input Status Register 1 - mono emulation */
#define VGA_PEL_D 0x3C9   /* PEL Data Register */
#define VGA_PEL_MSK 0x3C6 /* PEL mask register */

/* 8514/MACH regs we need outside of the mach32 driver.. */
#define PEL8514_D 0x2ED
#define PEL8514_IW 0x2EC
#define PEL8514_IR 0x2EB
#define PEL8514_MSK 0x2EA

/* EGA-specific registers */

#define VGA_GRA_E0 0x3CC /* Graphics enable processor 0 */
#define VGA_GRA_E1 0x3CA /* Graphics enable processor 1 */

/* standard VGA indexes max counts */
#define VGA_CRT_MAX 24 /* 24 CRT Controller Registers */
#define VGA_ATT_MAX 21 /* 21 Attribute Controller Registers */
#define VGA_GRA_MAX 9  /* 9  Graphics Controller Registers */
#define VGA_SEQ_MAX 5  /* 5  Sequencer Registers */
#define VGA_MIS_MAX 1  /* 1  Misc Output Register */

/* VGA registers saving indexes */
#define VGA_CRT 0                     /* CRT Controller Registers start */
#define VGA_ATT (VGA_CRT + VGA_CRT_C) /* Attribute Controller Registers start */
#define VGA_GRA (VGA_ATT + VGA_ATT_C) /* Graphics Controller Registers start */
#define VGA_SEQ (VGA_GRA + VGA_GRA_C) /* Sequencer Registers */
#define VGA_MIS (VGA_SEQ + VGA_SEQ_C) /* General Registers */
#define VGA_EXT (VGA_MIS + VGA_MIS_C) /* SVGA Extended Registers */

#define VGA_GRAPH_BASE 0xFD000000
// #define VGA_GRAPH_BASE 0xA0000
#define VGA_FONT_BASE 0xA0000
#define VGA_GRAPH_SIZE 0x10000
#define VGA_FONT_SIZE (0x2000 * 4) /* 2.0.x kernel can use 2 512 char. fonts */

extern unsigned char g_320x200x256[];
extern unsigned char g_320x200x256_modex[];
extern unsigned char g_640x480x16[];
extern unsigned char g_320x240x256_modex[];
extern unsigned char _mode13h_320x200x256[];

void write_regs(unsigned char *regs);
void setdefaultVGApalette(void);
// void *map_video_mem(u32_t width, u32_t height, u32_t col);
// void map_video_mem(u32_t width, u32_t height, u32_t col, u64_t framebf);

#endif