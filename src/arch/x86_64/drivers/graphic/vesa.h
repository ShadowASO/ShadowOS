/*--------------------------------------------------------------------------
*  File name:  vesa.h
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 21-11-2022
*--------------------------------------------------------------------------
Rotinas para manipulação de vídeo svga
--------------------------------------------------------------------------*/
#pragma once

#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "io.h"
#include "ktypes.h"
#include "hpet.h"

typedef struct vbe_info
{
  char signature[4];     // must be "VESA" to indicate valid VBE support
  uint16_t version;      // VBE version; high byte is major version, low byte is minor version
  uint32_t oem;          // segment:offset pointer to OEM
  uint32_t capabilities; // bitfield that describes card capabilities
  uint32_t video_modes;  // segment:offset pointer to list of supported video modes
  uint16_t video_memory; // amount of video memory in 64KB blocks
  uint16_t software_rev; // software revision
  uint32_t vendor;       // segment:offset to card vendor string
  uint32_t product_name; // segment:offset to card model name
  uint32_t product_rev;  // segment:offset pointer to product revision
  char reserved[222];    // reserved for future expansion
  char oem_data[256];    // OEM BIOSes store their strings in this area
} __attribute__((packed)) vbe_info_t;

typedef struct mode_info_block
{
  uint16_t attributes;
  uint8_t windowA, windowB;
  uint16_t granularity;
  uint16_t windowSize;
  uint16_t segmentA, segmentB;
  uint32_t winFuncPtr;
  uint16_t pitch;

  uint16_t resolutionX, resolutionY;
  uint8_t wChar, yChar, planes, bpp, banks;
  uint8_t memoryModel, bankSize, imagePages;
  uint8_t reserved0;

  uint8_t readMask, redPosition;
  uint8_t greenMask, greenPosition;
  uint8_t blueMask, bluePosition;
  uint8_t reservedMask, reservedPosition;
  uint8_t directColorAttributes;

  // linear frame buffer
  uint32_t physbase;
  uint32_t offScreenMemOff;
  uint16_t offScreenMemSize;
  uint8_t reserved1[206];
} mode_info_t;

typedef union uint24
{
  struct
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  } rgb;
  unsigned int integer : 24 __attribute__((packed));
  ;
} uint24_t;

// void vesa_memcpy24_32(uint24_t *dest, uint32_t *src, uint32_t count);
// void vesa_memset_rgb(uint8_t *dest, uint32_t rgb, uint32_t count);
// void *vesa_get_lfb(void);
// int vesa_get_resolution_x(void);
// int vesa_get_resolution_y(void);
// void vesa_get_mode(uint16_t mode, mode_info_t *mode_info);
// void vesa_set_mode(uint32_t mode);

extern u64_t vesa_realmode_start;
extern u64_t vesa_realmode_end;

extern vbe_info_t vbe_hinfo;
extern mode_info_t vbe_hmode;

extern uint16_t video_xbytes;
extern uint16_t video_xres;
extern uint16_t video_yres;
extern uint8_t *video_buffer;
extern uint32_t video_physbase;

void setup_vesa(void);
void map_video_mem(void);
void get_vbe_mode_info(u32_t base_ap);
void vesa_init(void);

void testa_map_video_mem(void);
void show_video_atribs(void);

// Rotinas em assembly e que só devem ser chamdas depois de executado o mode
// gráfico.
void *_get_vbe_mode_info(void);
void *_get_vbe_info(void);
u16_t _get_video_status(void); // Devolve o mode atual ou 0(zero), se houve falha.

/*Testa se a inicialização foi realizada corretamente. */
static inline bool is_video_ini_ok(void)
{
  return _get_video_status();
}
