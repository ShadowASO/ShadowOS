/*
Copyright (C) 2015-2019 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file LICENSE for details.
*/

#include "console.h"
#include "graphics.h"
#include "mm/kmalloc.h"
#include "string.h"
#include "ktypes.h"
#include "task.h"
#include "hpet.h"
#include "sync/mutex.h"
#include "debug.h"
#include "scheduler.h"

static bool graphic_mode_on = false;
console_t console_root = {0};
static struct graphics_color bgcolor = {0, 0, 0, 0};
static struct graphics_color fgcolor = {255, 255, 255, 0};

struct graphics *graphic_obj;
console_t *console_obj;

#define CURSOR_PULSE_MS_TIME 300
static u64_t time_ini = 0;
static u64_t time_ms = 0;

CREATE_SPINLOCK(spinlock_console);

void set_graphic_mode(bool mode)
{
	graphic_mode_on = mode;
}
bool is_graphic_mode(void)
{
	return graphic_mode_on;
}

void console_clear_screen(console_t *d)
{
	if (!d || !d->gx)
		return;

	graphics_clear(d->gx, 0, 0, graphics_width(d->gx), graphics_height(d->gx));
	console_obj->xpos = 0;
	console_obj->ypos = 0;
}

static void console_reset(console_t *d)
{
	if (!d || !d->gx)
		return;
	d->xpos = d->ypos = 0;
	//*****
	d->xpos_cur = d->xpos;
	d->ypos_cur = d->ypos;
	//***

	d->xsize = graphics_width(d->gx) / 8;
	d->ysize = graphics_height(d->gx) / 8;
	d->onoff = 0;
	d->key_tab = ESC_TAB_SIZE;
	graphics_fgcolor(d->gx, fgcolor);
	graphics_bgcolor(d->gx, bgcolor);
}
/* Faz um cursor piscar na tela em modo gráfico. */
void console_heartbeat(console_t *d)
{
	char c = d->onoff ? ' ' : '_';

	if (d->onoff)
	{
		graphics_char(d->gx, d->xpos * 8, d->ypos * 8, c);
	}
	else
	{
		graphics_char(d->gx, d->xpos * 8, d->ypos * 8, c);
	}
	d->onoff = !d->onoff;
}

int console_write(console_t *d, const char *data, int size)
{
	graphics_char(d->gx, d->xpos * 8, d->ypos * 8, ' ');

	int i;
	for (i = 0; i < size; i++)
	{
		char c = data[i];
		switch (c)
		{
		case 13:
		case 10:
			// case 10:
			d->xpos = 0;
			d->ypos++;
			break;
		case ESC_TAB:
			// d->xpos += d->key_tab;
			d->xpos = ((u8_t)(d->xpos / d->key_tab) * d->key_tab + d->key_tab);
			break;
		case '\f':
			d->xpos = d->ypos = 0;
			d->xsize = graphics_width(d->gx) / 8;
			d->ysize = graphics_height(d->gx) / 8;
			graphics_fgcolor(d->gx, fgcolor);
			graphics_bgcolor(d->gx, bgcolor);
			graphics_clear(d->gx, 0, 0, graphics_width(d->gx), graphics_height(d->gx));
			break;
		case ESC_BACKSPACE:
			d->xpos--;
			break;
		default:
			graphics_char(d->gx, d->xpos * 8, d->ypos * 8, c);
			d->xpos++;
			break;
		}

		if (d->xpos < 0)
		{
			d->xpos = d->xsize - 1;
			d->ypos--;
		}

		if (d->xpos >= d->xsize)
		{
			d->xpos = 0;
			d->ypos++;
		}

		if (d->ypos >= d->ysize)
		{
			d->xpos = d->ypos = 0;
			d->xsize = graphics_width(d->gx) / 8;
			d->ysize = graphics_height(d->gx) / 8;
			graphics_fgcolor(d->gx, fgcolor);
			graphics_bgcolor(d->gx, bgcolor);
			graphics_clear(d->gx, 0, 0, graphics_width(d->gx), graphics_height(d->gx));
		}
	}

	graphics_char(d->gx, d->xpos * 8, d->ypos * 8, ' ');
	return i;
}
//*******
/**
 * @brief Esta versão faz a exibição no monitor, mas sem atualizar as posições de linha
 * e coluna, o que evita termos que fazer atualização, o que estava gerando erro por
 * concorrência da rotina do cursor e do time.
 *
 * @param d
 * @param col
 * @param row
 * @param data
 * @param size
 * @return int
 */
static int console_write_at(console_t *d, int col, int row, const char *data, int size)
{
	graphics_char(d->gx, col * 8, row * 8, ' ');
	int xpos = col;
	int ypos = row;

	int i;
	for (i = 0; i < size; i++)
	{
		char c = data[i];
		switch (c)
		{
		case 13:
		case 10:
			// case 10:
			xpos = 0;
			ypos++;
			break;
		case ESC_TAB:
			// xpos += d->key_tab;
			xpos = ((u8_t)(xpos / d->key_tab) * d->key_tab + d->key_tab);
			break;
		case '\f':
			xpos = ypos = 0;
			d->xsize = graphics_width(d->gx) / 8;
			d->ysize = graphics_height(d->gx) / 8;
			graphics_fgcolor(d->gx, fgcolor);
			graphics_bgcolor(d->gx, bgcolor);
			graphics_clear(d->gx, 0, 0, graphics_width(d->gx), graphics_height(d->gx));
			break;
		case ESC_BACKSPACE:
			xpos--;
			break;
		default:
			graphics_char(d->gx, xpos * 8, ypos * 8, c);
			xpos++;
			break;
		}

		if (xpos < 0)
		{
			xpos = d->xsize - 1;
			ypos--;
		}

		if (xpos >= d->xsize)
		{
			xpos = 0;
			ypos++;
		}

		if (ypos >= d->ysize)
		{
			xpos = ypos = 0;
			d->xsize = graphics_width(d->gx) / 8;
			d->ysize = graphics_height(d->gx) / 8;
			graphics_fgcolor(d->gx, fgcolor);
			graphics_bgcolor(d->gx, bgcolor);
			graphics_clear(d->gx, 0, 0, graphics_width(d->gx), graphics_height(d->gx));
		}
	}
	// graphics_char(d->gx, xpos * 8, ypos * 8, '_');
	graphics_char(d->gx, xpos * 8, ypos * 8, ' ');
	return i;
}
/**
 * @brief Escreve no monitor, sem atualizar os valores de coluna e linha.
 *
 * @param c
 * @param col
 * @param row
 * @param ch
 */
void console_putchar_at(console_t *c, int col, int row, char ch)
{
	// spinlock_lock(&c->spinlock_tty);
	console_write_at(c, col, row, &ch, 1);
	// spinlock_unlock(&c->spinlock_tty);
}
/**
 * @brief Escreve no monitor sem atualizar os valores de linha e coluna.
 *
 * @param c
 * @param col
 * @param row
 * @param str
 */
void console_putstring_at(console_t *c, int col, int row, const char *str)
{
	// spinlock_lock(&c->spinlock_tty);

	console_write_at(c, col, row, str, strlen(str));

	// spinlock_unlock(&c->spinlock_tty);
}
void console_clear_line(console_t *c)
{
	graphics_clear(c->gx, 0, c->ypos * 8, graphics_width(c->gx), 8);
	c->xpos = 0;
}
void console_cursor_set(console_t *d, int col, int lin)
{
	d->xpos_cur = col;
	d->ypos_cur = lin;
}

//*****
void console_putchar(console_t *c, char ch)
{
	console_write(c, &ch, 1);
}

void console_putstring(console_t *c, const char *str)
{
	console_write(c, str, strlen(str));
}

console_t *console_create(struct graphics *g)
{
	console_t *c = kmalloc(sizeof(*c));

	c->gx = graphics_addref(g);
	c->refcount = 1;
	console_reset(c);
	return c;
}

console_t *console_addref(console_t *c)
{
	c->refcount++;
	return c;
}

void console_delete(console_t *c)
{
	c->refcount--;
	if (c->refcount == 0)
	{
		graphics_delete(c->gx);
		if (c != &console_root)
			kfree(c);
	}
}

void console_size(console_t *c, int *xsize, int *ysize)
{
	*xsize = c->xsize;
	*ysize = c->ysize;
}

console_t *console_init(struct graphics *g)
{
	console_root.gx = g;
	console_reset(&console_root);
	console_putstring(&console_root, "\nconsole: initialized\n");

	return &console_root;
}
