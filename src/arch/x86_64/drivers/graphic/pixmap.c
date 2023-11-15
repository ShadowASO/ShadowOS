/*
Copyright (C) 2016-2019 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file LICENSE for details.
*/

#include "pixmap.h"
#include "mm/kmalloc.h"
#include "vesa.h"
#include "debug.h"

static pixmap_t root_pixmap;

pixmap_t *pixmap_create_root(void)
{
	root_pixmap.width = video_xres;
	root_pixmap.height = video_yres;
	root_pixmap.format = BITMAP_FORMAT_RGB;
	root_pixmap.data = video_buffer;
	return &root_pixmap;
}
void set_pixmap_root_attribs(u32_t width, u32_t height, virt_addr_t buffer)
{
	root_pixmap.width = width;
	root_pixmap.height = height;
	root_pixmap.format = BITMAP_FORMAT_RGB;
	root_pixmap.data = buffer;
}
pixmap_t *get_pixmap_root(void)
{
	return &root_pixmap;
}

pixmap_t *pixmap_create(int width, int height, int format)
{
	pixmap_t *b = kmalloc(sizeof(*b));

	if (!b)
		return 0;

	b->data = kmalloc(width * height * 3);
	if (!b->data)
	{
		kfree(b);
		return 0;
	}

	b->width = width;
	b->height = height;
	b->format = format;

	return b;
}

void pixmap_delete(pixmap_t *b)
{
	kfree(b->data);
	kfree(b);
}
