/*
Copyright (C) 2015-2019 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file LICENSE for details.
*/

#ifndef CONSOLE_H
#define CONSOLE_H

#include "graphics.h"
#include "string.h"
#include "sync/spin.h"

// Scape caracteres
#define __CHAR_SPACE ' '
#define ESC_NEWLINE '\n'
#define ESC_TAB '\t'
#define ESC_BACKSPACE '\b'

// Atributos caracteres
#define ESC_TAB_SIZE 4
#define CHAR_WIDTH 8
#define CHAR_HIGHT 8

typedef struct _console
{
    struct graphics *gx;
    int xsize;
    int ysize;
    int xpos;
    int ypos;
    int xpos_cur;
    int ypos_cur;
    int onoff;
    int key_tab;
    int refcount;
    spinlock_t spinlock_tty;
} console_t;

/*
console_init creates the very first global console that
is used for kernel debug output.  The singleton "console_root"
must be statically allocated so that is usable at early
startup before memory allocation is available.
*/

// extern struct console console_root;
extern console_t console_root;
extern struct graphics *graphic_obj;
// extern struct console *console_obj;
extern console_t *console_obj;
// struct console *console_init(struct graphics *g);
console_t *console_init(struct graphics *g);

/*
Any number of other consoles can be created and manipulated
on top of existing windows.
*/

console_t *console_create(struct graphics *g);
void console_delete(console_t *c);
int console_write(console_t *c, const char *data, int length);
void console_putchar(console_t *c, char ch);
void console_putstring(console_t *c, const char *str);
void console_heartbeat(console_t *c);
void console_size(console_t *c, int *xsize, int *ysize);
console_t *console_addref(console_t *c);

void set_graphic_mode(bool mode);
bool is_graphic_mode(void);

void console_putchar_at(console_t *c, int col, int row, char ch);
void console_putstring_at(console_t *c, int col, int row, const char *str);
void console_cursor_set(console_t *d, int col, int lin);
void console_clear_line(console_t *c);
void console_clear_screen(console_t *d);

static inline void console_cursor_left(console_t *d)
{
    if (d->xpos > 0)
        d->xpos--;
}
static inline void console_cursor_right(console_t *d)
{
    if (d->xpos < (d->xsize / CHAR_WIDTH) - 1)
        d->xpos++;
}
static inline void console_cursor_col_set(console_t *d, int col)
{
    d->xpos = col;
}
static inline u64_t console_cursor_col_get(console_t *d)
{
    return d->xpos;
}

#endif
