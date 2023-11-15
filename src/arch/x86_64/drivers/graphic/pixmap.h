/*
Copyright (C) 2016-2019 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file LICENSE for details.
*/

#ifndef _PIXMAP_H
#define _PIXMAP_H

#include "ktypes.h"

typedef struct pixmap
{
	uint32_t width;
	uint32_t height;
	uint32_t format;
	uint8_t *data;
} pixmap_t;

pixmap_t *pixmap_create_root(void);
pixmap_t *pixmap_create(int width, int height, int format);
void pixmap_delete(pixmap_t *b);
pixmap_t *get_pixmap_root(void);

void set_pixmap_root_attribs(u32_t width, u32_t height, virt_addr_t buffer);

#define BITMAP_FORMAT_RGB 0
#define BITMAP_FORMAT_RGBA 1

extern uint16_t total_memory;
// extern uint32_t kernel_size;

#endif
