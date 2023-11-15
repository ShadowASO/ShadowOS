/*--------------------------------------------------------------------------
*  File name:  shell.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 05-02-2021
*  Revisão: 21-05-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui o código de um shell
--------------------------------------------------------------------------*/
#include "stdlib.h"
#include "stdio.h"
#include "keyboard.h"
#include "screen.h"
#include "string.h"
#include "shell.h"
#include "ascii.h"
#include "debug.h"
#include "ktypes.h"
#include "mm/kmalloc.h"
#include "../drivers/graphic/console.h"
#include "shell_rotinas.h"
#include "../user/read.h"

/* Variáveis relacionadas ao Timer e Clock. */
struct graphics *g_clock;
console_t *tty_clock = NULL;

/* Uso genérico. */
struct graphics *g_obj1;
console_t *tty1 = NULL;

struct timer_list clock_timer;
struct timer_list cursor_timer;

/*---------------------------------------------*/

static char *prompt = "=> ";
static int curr_col = 0;
static int curr_row = 0;

/* Vetor de ponteiro para o history*/
#define HISTORY_COUNT 20
#define CMD_SIZE 30

static char history[HISTORY_COUNT][CMD_SIZE + 1] = {0};
static int next_cmd = 0;
static int last_cmd = 0;

/*--------------------------------------*/
static void shell_loop(void);
static void shell_read(char *buf);
static void show_prompt(void);
static void show_newline(void);
static void timer_handler_cursor(u64_t data);
static void timer_handler_clock(u64_t data);
static void update_local_clock(void);
/*----------------------------------------*/

void shell_ini(void)
{

    if (is_graphic_mode())
    {
        g_obj1 = graphics_create(graphic_obj);
        tty1 = console_create(g_obj1);
        INIT_SPINLOCK(&tty1->spinlock_tty);

        g_clock = graphics_create(graphic_obj);
        tty_clock = console_create(g_clock);
        INIT_SPINLOCK(&tty_clock->spinlock_tty);

        tty1->xpos = 0;
        tty1->ypos = 1;
    }

    /* Relógio do terminal.                     */
    load_new_timer(&clock_timer, timer_handler_clock, ((mm_addr_t)&clock_timer), (get_jiffies() + TIME_MILISEC));
    /*------------------------------------------*/

    /* Faço o cursor piscar, utilizando um  virtual timer.  500 milissegundos de delay */
    load_new_timer(&cursor_timer, timer_handler_cursor, ((mm_addr_t)&cursor_timer), (get_jiffies() + TIME_MILISEC / 2));

    shell_loop();
}

static void shell_loop(void)
{
    char line[124];
    const char *argv[124];
    int argc;

    while (true)
    {
        show_newline();

        show_prompt();

        shell_read(line);

        argc = 0;
        argv[argc] = strtok(line, " ");

        while (argv[argc])
        {
            argc++;
            argv[argc] = strtok(0, " ");
        }
        if (argc > 0)
        {
            last_cmd = next_cmd;
            strncpy(history[next_cmd++], argv[0], 30);
            next_cmd = next_cmd % HISTORY_COUNT;

            kshell_execute(argc, argv);
            memset(line, 0, sizeof(line));
        }
    }
}

/* loop que permite a edição de uma linha no prompt. */
static void shell_read(char *buffer)
{
    int line_size = 0;
    int i = 0;
    char c = NULL_CHAR;
    int cursor_ini = 0;

    size_t x = 0;

    /* Pego a posição atual do cursor. */
    cursor_ini = console_cursor_col_get(console_obj);

    do
    {
        // c = kgetch();
        // c = syscall_exec(__NR_read, 0, 0, 0, 0, 0);
        c = getc();

        if (c == KEY_LEFT && i > 0)
        {
            i--;
        }
        else if (c == KEY_RIGHT && i < line_size)
        {
            i++;
        }
        else if (c == KEY_END)
        {
            i = line_size;
        }
        else if (c == KEY_UP)
        {
            if (last_cmd > 0)
            {
                last_cmd--;
                strcpy(buffer, history[last_cmd]);
                i = strlen(buffer);
                line_size = i;
            }
        }
        else if (c == KEY_DOWN)
        {
            if (last_cmd < HISTORY_COUNT)
            {
                last_cmd++;
                strcpy(buffer, history[last_cmd]);
                i = strlen(buffer);
                line_size = i;
            }
        }
        else if (c == KEY_HOME)
        {
            i = 0;
        }
        else if (c == KEY_BACKSPACE && i > 0)
        {
            i--;
            line_size--;
            strndel(buffer, i);
        }
        else if (c == KEY_DELETE && i > 0)
        {
            strndel(buffer, i);
            line_size--;
        }
        else if (!iscntrl(c))
        {
            // Inserindo um caractere no meio de uma string
            if (i < line_size)
            {
                strnins(buffer, c, i);
                line_size++;
                i++;
            }
            else
            {
                buffer[i] = c;
                i++;
                line_size++;
                buffer[line_size] = '\0';
            }
        }

        // Todas as condições anteriores tem reflexo na string e precisam ser exibidos.
        if (is_graphic_mode())
        {
            console_clear_line(console_obj);
            show_prompt();
            console_putstring(console_obj, buffer);
            console_cursor_col_set(console_obj, (cursor_ini + i));
        }
        else
        {
            cls_current_line(curr_row);
            show_prompt();
            printf("%s", buffer);
        }
        while (x > 100)
        {
            /* code */
        }
        x++;

    } while (c != KEY_ENTER);
}

static void show_prompt(void)
{
    curr_col = 0;

    if (!is_graphic_mode())
    {
        curr_row = get_screen_cursor_row();
        set_cursor_pos(curr_row, 0);
    }
    printf("%s", prompt);
}

static void show_newline(void)
{
    curr_col = 0;
    curr_row++;
    if (is_graphic_mode())
    {
        printf("\n");
    }
    else // Modo texto
    {
        curr_row = get_screen_cursor_row();
        set_cursor_pos(curr_row, curr_col);
        printf("\n");
    }
}

/* Timer handler que faz o cursor piscar. */
static void timer_handler_cursor(u64_t data)
{
    console_heartbeat(console_obj);

    /* Re-armo o timer do relógio*/
    mod_timer(&cursor_timer, get_jiffies() + TIME_MILISEC / 2); // 500 milissegundos de delay
    add_timer(&cursor_timer);
}

static void timer_handler_clock(u64_t data)
{
    static u64_t seconds = 0;
    static u64_t curr_time = 0;
    static char *time_str = "%u-%u-%u - %u:%u:%u";
    static char buf_clock[32] = {0};

    int row = 0;
    int time_len = 0;
    int col_end = MAX_COLS / 2;

    tm_t utc = timestamp_to_utc(xtime.tv_sec);

    update_local_clock();

    // sprintf(buf_clock, time_str, rtc.year, rtc.mth, rtc.day, rtc.hr, rtc.min, rtc.sec);
    sprintf(buf_clock, time_str, utc.tm_day, utc.tm_mon, utc.tm_year, utc.tm_hour, utc.tm_min, utc.tm_sec);
    time_len = strlen(buf_clock) / 2;

    if (is_graphic_mode())
    {
        console_putstring_at(tty1, (col_end - time_len), row, buf_clock);
    }
    else
    {
        print_at(buf_clock, row, (col_end - time_len));
    }

    /* Re-armo o timer do relógio*/

    mod_timer(&clock_timer, get_jiffies() + TIME_MILISEC);
    add_timer(&clock_timer);
}

static void update_local_clock(void)
{
    rtc.sec++;
    // xtime.tv_sec

    if (rtc.sec == 60)
    {
        rtc.sec = 0;
        rtc.min++;
    }
    if (rtc.min == 60)
    {
        rtc.min = 0;
        rtc.hr++;
    }
    if (rtc.hr == 24)
    {
        rtc.hr = 0;
        rtc.day++;
    }
}
