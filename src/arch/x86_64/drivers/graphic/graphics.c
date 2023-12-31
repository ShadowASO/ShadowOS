/*
Copyright (C) 2016-2019 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file LICENSE for details.
*/

#include "graphics.h"
// #include "kernel/types.h"
// #include "ioports.h"
#include "font.h"
#include "string.h"
#include "mm/kmalloc.h"
#include "pixmap.h"
#include "string.h"
// #include "process.h"
#include "ktypes.h"
#include "io.h"
// #include "utils.h"
#include "stdio.h"
#include "debug.h"

#define FACTOR 256

struct graphics_clip
{
	u32_t x;
	u32_t y;
	u32_t w;
	u32_t h;
};

struct graphics
{
	pixmap_t *pixmap;
	struct graphics_color fgcolor;
	struct graphics_color bgcolor;
	struct graphics_clip clip;
	struct graphics *parent;
	u32_t refcount;
};

static struct graphics_color color_black = {0, 0, 0, 0};
static struct graphics_color color_white = {255, 255, 255, 0};

struct graphics graphics_root;

struct graphics *graphics_create_root()
{
	struct graphics *g = &graphics_root;
	g->pixmap = pixmap_create_root();
	g->fgcolor = color_white;
	g->bgcolor = color_black;
	g->clip.x = 0;
	g->clip.y = 0;
	g->clip.w = g->pixmap->width;
	g->clip.h = g->pixmap->height;
	g->parent = 0;
	g->refcount = 1;
	return g;
}

struct graphics *graphics_create(struct graphics *parent)
{

	struct graphics *g = kmalloc(sizeof(*g));

	if (!g)
		return 0;

	memcpy(g, parent, sizeof(*g));

	g->parent = graphics_addref(parent);
	g->refcount = 1;

	return g;
}

struct graphics *graphics_addref(struct graphics *g)
{
	g->refcount++;
	return g;
}

void graphics_delete(struct graphics *g)
{
	if (!g)
		return;

	/* Cannot delete the statically allocated root graphics */
	if (g == &graphics_root)
		return;

	g->refcount--;
	if (g->refcount == 0)
	{
		graphics_delete(g->parent);
		kfree(g);
	}
}

u32_t graphics_write(struct graphics *g, struct graphics_command *command)
{
	int window = -1;
	char *str;
	struct graphics_color c;

	while (command && command->type)
	{
		switch (command->type)
		{
		case GRAPHICS_WINDOW:
			window = command->args[0];
			if (window < 0)
			{
				return -1;
			}
			break;
		case GRAPHICS_COLOR:
			c.r = command->args[0];
			c.g = command->args[1];
			c.b = command->args[2];
			c.a = 0;
			graphics_fgcolor(g, c);
			break;
		case GRAPHICS_RECT:
			graphics_rect(g, command->args[0], command->args[1], command->args[2], command->args[3]);
			break;
		case GRAPHICS_CLEAR:
			graphics_clear(g, command->args[0], command->args[1], command->args[2], command->args[3]);
			break;
		case GRAPHICS_LINE:
			graphics_line(g, command->args[0], command->args[1], command->args[2], command->args[3]);
			break;
		case GRAPHICS_TEXT:
			str = (char *)(command->args[2]);
			u32_t i;
			for (i = 0; str[i]; i++)
			{
				graphics_char(g, command->args[0] + i * 8, command->args[1], str[i]);
			}
			break;
		case GRAPHICS_END:
			break;
		default:
			break;
		}
		command++;
	}
	return 0;
}

u32_t graphics_width(struct graphics *g)
{
	return g->clip.w;
}

u32_t graphics_height(struct graphics *g)
{
	return g->clip.h;
}

void graphics_fgcolor(struct graphics *g, struct graphics_color c)
{
	g->fgcolor = c;
}

void graphics_bgcolor(struct graphics *g, struct graphics_color c)
{
	g->bgcolor = c;
}

u32_t graphics_clip(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h)
{
	// Clip values may not be negative
	// if (x < 0 || y < 0 || w < 0 || h < 0)
	//	return 0;

	// Child origin is relative to parent's clip origin.
	x += g->clip.x;
	y += g->clip.y;

	// Child origin must fall within parent clip
	if (x >= g->pixmap->width || y >= g->pixmap->width)
		return 0;

	// Child width must fall within parent size
	if ((x + w) >= g->pixmap->width || (y + h) >= g->pixmap->height)
		return 0;

	// Apply the clip
	g->clip.x = x;
	g->clip.y = y;
	g->clip.w = w;
	g->clip.h = h;
	return 1;
}

static inline void plot_pixel(struct pixmap *bmap, u32_t x, u32_t y, struct graphics_color c)
{
	u8_t *v = bmap->data + (bmap->width * y + x) * 3;
	if (c.a == 0)
	{
		v[2] = c.r;
		v[1] = c.g;
		v[0] = c.b;
	}
	else
	{
		u16_t a = c.a;
		u16_t b = 256 - a;
		v[0] = (c.r * b + v[0] * a) >> 8;
		v[1] = (c.g * b + v[1] * a) >> 8;
		v[2] = (c.b * b + v[2] * a) >> 8;
	}
	RSP_MAP();
}

void graphics_rect(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h)
{
	u32_t i, j;

	w = min(g->clip.w - x, w);
	h = min(g->clip.h - y, h);
	x += g->clip.x;
	y += g->clip.y;

	for (j = 0; j < h; j++)
	{
		for (i = 0; i < w; i++)
		{
			plot_pixel(g->pixmap, x + i, y + j, g->fgcolor);
		}
	}
}

void graphics_clear(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h)
{
	u32_t i, j;

	w = min(g->clip.w - x, w);
	h = min(g->clip.h - y, h);
	x += g->clip.x;
	y += g->clip.y;

	for (j = 0; j < h; j++)
	{
		for (i = 0; i < w; i++)
		{
			plot_pixel(g->pixmap, x + i, y + j, g->bgcolor);
		}
	}
}

static inline void graphics_line_vert(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h)
{
	do
	{
		plot_pixel(g->pixmap, x, y, g->fgcolor);
		y++;
		h--;
	} while (h > 0);
}

static inline void graphics_line_q1(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h)
{
	u32_t slope = FACTOR * w / h;
	u32_t counter = 0;

	do
	{
		plot_pixel(g->pixmap, x, y, g->fgcolor);
		y++;
		h--;
		counter += slope;
		if (counter > FACTOR)
		{
			counter = counter - FACTOR;
			x++;
			w--;
		}
	} while (h > 0);
}

static inline void graphics_line_q2(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h)
{
	u32_t slope = FACTOR * h / w;
	u32_t counter = 0;

	do
	{
		plot_pixel(g->pixmap, x, y, g->fgcolor);
		x++;
		w--;
		counter += slope;
		if (counter > FACTOR)
		{
			counter = counter - FACTOR;
			y++;
			h--;
		}
	} while (w > 0);
}

static inline void graphics_line_q3(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h)
{
	u32_t slope = -FACTOR * h / w;
	u32_t counter = 0;

	do
	{
		plot_pixel(g->pixmap, x, y, g->fgcolor);
		x++;
		w--;
		counter += slope;
		if (counter > FACTOR)
		{
			counter = counter - FACTOR;
			y--;
			h--;
		}
	} while (w > 0);
}

static inline void graphics_line_q4(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h)
{
	u32_t slope = -FACTOR * w / h;
	u32_t counter = 0;

	do
	{
		plot_pixel(g->pixmap, x, y, g->fgcolor);
		y--;
		h--;
		counter += slope;
		if (counter > FACTOR)
		{
			counter = counter - FACTOR;
			x++;
			w--;
		}
	} while (h > 0);
}

static inline void graphics_line_hozo(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h)
{
	do
	{
		plot_pixel(g->pixmap, x, y, g->fgcolor);
		x++;
		w--;
	} while (w > 0);
}

void graphics_line(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h)
{
	/*
	if (w < 0)
	{
		x = x + w;
		y = y + h;
		w = -w;
		h = -h;
	}*/

	x += g->clip.x;
	y += g->clip.y;

	if (h > 0)
	{
		if (w == 0)
		{
			graphics_line_vert(g, x, y, w, h);
		}
		else if (h > w)
		{
			graphics_line_q1(g, x, y, w, h);
		}
		else
		{
			graphics_line_q2(g, x, y, w, h);
		}
	}
	else
	{
		if (h == 0)
		{
			graphics_line_hozo(g, x, y, w, h);
		}
		else if (h > -w)
		{
			graphics_line_q3(g, x, y, w, h);
		}
		else
		{
			graphics_line_q4(g, x, y, w, h);
		}
	}
}

void graphics_pixmap(struct graphics *g, u32_t x, u32_t y, u32_t width, u32_t height, u8_t *data)
{
	u32_t i, j, b;
	int value;

	width = min(g->clip.w - x, width);
	height = min(g->clip.h - y, height);
	x += g->clip.x;
	y += g->clip.y;

	b = 0;

	for (j = 0; j < height; j++)
	{
		for (i = 0; i < width; i++)
		{
			value = ((*data) << b) & 0x80;
			if (value)
			{
				plot_pixel(g->pixmap, x + i, y + j, g->fgcolor);
			}
			else
			{
				plot_pixel(g->pixmap, x + i, y + j, g->bgcolor);
			}
			b++;
			if (b == 8)
			{
				data++;
				b = 0;
			}
		}
	}
	RSP_MAP();
}

void graphics_char(struct graphics *g, u32_t x, u32_t y, unsigned char c)
{
	u32_t u = ((u32_t)c) * FONT_WIDTH * FONT_HEIGHT / 8;

	return graphics_pixmap(g, x, y, FONT_WIDTH, FONT_HEIGHT, &fontdata[u]);
}

void graphics_scrollup(struct graphics *g, u32_t x, u32_t y, u32_t w, u32_t h, u32_t dy)
{
	u32_t j;

	w = min(g->clip.w - x, w);
	h = min(g->clip.h - y, h);
	x += g->clip.x;
	y += g->clip.y;

	if (dy > h)
		dy = h;

	for (j = 0; j < (h - dy); j++)
	{
		memcpy(&g->pixmap->data[((y + j) * g->pixmap->width + x) * 3], &g->pixmap->data[((y + j + dy) * g->pixmap->width + x) * 3], w * 3);
	}

	graphics_clear(g, x, y + h - dy, w, dy);
}
