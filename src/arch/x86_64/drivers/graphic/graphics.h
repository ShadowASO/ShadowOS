/*
Copyright (C) 2016-2019 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file LICENSE for details.
*/

#ifndef GRAPHICS_H
#define GRAPHICS_H

//#include "kernel/types.h"
//#include "kernel/gfxstream.h"
#include "ktypes.h"
#include "gfxstream.h"

struct graphics_color
{
	u32_t r;
	u32_t g;
	u32_t b;
	u32_t a;
};

extern struct graphics graphics_root;

struct graphics *graphics_create_root(void);
struct graphics *graphics_create(struct graphics *parent);
struct graphics *graphics_addref(struct graphics *g);
void graphics_delete(struct graphics *g);

u32_t graphics_width(struct graphics *g);
u32_t graphics_height(struct graphics *g);
void graphics_fgcolor(struct graphics *g, struct graphics_color c);
void graphics_bgcolor(struct graphics *g, struct graphics_color c);
// u32_t graphics_clip(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h);
u32_t graphics_clip(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h);
void graphics_pixmap(struct graphics *g, u32_t x, u32_t y, u32_t width, u32_t height, u8_t *data);

void graphics_scrollup(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h, u32_t dy);
// void graphics_rect(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h);
void graphics_rect(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h);
// void graphics_clear(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h);
void graphics_clear(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h);
void graphics_line(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h);
void graphics_char(struct graphics *g, u32_t x, u32_t y, unsigned char c);

u32_t graphics_write(struct graphics *g, struct graphics_command *command);

#endif
