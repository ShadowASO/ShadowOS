/*--------------------------------------------------------------------------
 *  File name:  screen.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 22-07-2020
 *  Revisão: 23-10-2022
 *--------------------------------------------------------------------------
 *  Rotinas para exibir texto no monitor.
 *--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "stdlib.h"
#include "screen.h"
#include "io.h"
#include "sync/mutex.h"
#include "../drivers/graphic/graphics.h"
#include "../drivers/graphic/console.h"

// Defino um tipo frame e o inicializo
PRIVATE screen_frame_t frame = {0, 0, MAX_COLS, MAX_ROWS};
ScreenWord_t VideoChar = {' ', {cWhite, cBlack}};
static ScreenWord_t VIDEO_BUFFER[25 * 80];
static ScreenWord_t *vidmem = (ScreenWord_t *)0xFFFFFFFF800B8000;
PRIVATE int screen_tab_size = 5;
CharColor_t VideoColor;

CREATE_SPINLOCK(spinlock_screen);
CREATE_SPINLOCK(spinlock_cursor);
CREATE_SPINLOCK(spinlock_vbuffer);

// Calculo a posição(offset) dentro do vetor composto pelo resultado de 80 x 25 = 2000
PRIVATE inline int
calcule_offset(int row, int col)
{
    int result = (row * MAX_COLS + col);
    return result;
}
// Obtenho a linha do cursor, a partir da posição dentro da matriz de memória 80 x 25 = 2000
PRIVATE inline int calcule_row(int offset)
{
    return offset / (MAX_COLS);
}

// Obtenho a coluna do cursor, a partir da posição dentro da matriz de memória 80 x 25 = 2000
PRIVATE inline int calcule_col(int offset)
{
    return (offset - (calcule_row(offset) * MAX_COLS));
}

/**
 * @brief Aplica um novo valor de offset ao cursos, que é deslocado
 * na tela.
 *
 * @param offset
 * @return PRIVATE
 */
PRIVATE void set_screen_cursor_pos(int offset)
{
    spinlock_lock(&spinlock_cursor);

    offset /= 1; // 2; // Convert from cell offset to char offset .
    // This is similar to get_cursor , only now we write
    // bytes to those internal device registers .
    __read_portb(REG_RESET_0x3C0);
    __write_portb(VGA_REG_ADDR, IO_HIG_BYTE);
    __write_portb(VGA_REG_DATA, (unsigned char)(offset >> 8));
    __write_portb(VGA_REG_ADDR, IO_LOW_BYTE);
    __write_portb(VGA_REG_DATA, (unsigned char)(offset & 0xff));

    spinlock_unlock(&spinlock_cursor);
}

/**
 * @brief Devolve o offset da posição atualmente ocupada pelo cursor
 * na memória de vídeo.
 *
 * @return PRIVATE
 */
PRIVATE int get_screen_cursor_pos(void)
{
    spinlock_lock(&spinlock_cursor);

    // The device uses its control register as an index
    // to select its internal registers , of which we are
    // interested in :
    // reg 14: which is the high byte of the cursor ’s offset
    // reg 15: which is the low byte of the cursor ’s offset
    // Once the internal register has been selected , we may read or
    // write a byte on the data register .
    __read_portb(REG_RESET_0x3C0);
    __write_portb(VGA_REG_ADDR, IO_HIG_BYTE);
    int offset = __read_portb(VGA_REG_DATA) << 8;
    __write_portb(VGA_REG_ADDR, IO_LOW_BYTE);
    offset += __read_portb(VGA_REG_DATA);
    // Since the cursor offset reported by the VGA hardware is the
    // number of characters , we multiply by two to convert it to
    // a character cell offset .

    spinlock_unlock(&spinlock_cursor);

    return offset;
}

/**
 * @brief  Fixa o frame da tela de exibição
 * @note
 * @param  col_min:
 * @param  row_min:
 * @param  col_max:
 * @param  row_max:
 * @retval None
 */
void set_screen_frame(int col_min, int row_min, int col_max, int row_max)
{
    frame.col_min = col_min;
    frame.row_min = row_min;
    frame.col_max = col_max;
    frame.row_max = row_max;
}

void set_screen_tab_size(int space_lenght)
{
    if (space_lenght > 5)
        screen_tab_size = space_lenght;
};

/**
 * @brief Verifica se a posição está dentro dos limites 24 X 80
 *
 * @param row
 * @param col
 * @return true
 * @return false
 */
static bool is_valid_screen_pos(int row, int col)
{
    bool result = true;
    if ((col < 0 && row < 0) || (col > MAX_COLS && row > MAX_ROWS))
    {
        result = false;
    }
    return result;
}

int get_screen_cursor_row(void)
{
    int offset = get_screen_cursor_pos();
    return calcule_row(offset);
}

int get_screen_cursor_col(void)
{
    int offset = get_screen_cursor_pos();
    return calcule_col(offset);
}
int set_cursor_pos(int row, int col)
{
    int offset = calcule_offset(row, col);
    set_screen_cursor_pos(offset);
    return offset;
}

/**
 * @brief  Faz a real gravação do texto diretamente na memória de vídeo.
 * @note   Escrevo apenas o caractere, sem o atributo de cor para evitar o flick da tela
 * @param  value:
 * @param  row:
 * @param  col:
 * @retval None
 */
PRIVATE inline void write_screen_at(char value, int row, int col)
{
    if (is_graphic_mode())
    {
        graphics_char(console_obj->gx, col * 8, row * 8, VIDEO_BUFFER[row * (MAX_COLS) + col].c);
    }
    else
    {
        spinlock_lock(&spinlock_screen);
        vidmem[(row * MAX_COLS) + col].c = value;
        spinlock_unlock(&spinlock_screen);
    }
}
PRIVATE inline void clear_screen_line(int row)
{
    for (int col = 0; col < MAX_COLS; col++)
    {
        write_screen_at(__CHAR_SPACE, row, col);
    }
}

PRIVATE inline void write_screen_buffer(char value, int row, int col)
{
    spinlock_lock(&spinlock_vbuffer);

    // Esta linha é redundante e utilizada apenas para fazer a
    // aplicação dos atributos de cor
    vidmem[(row * MAX_COLS) + col] = VideoChar;
    // Esta linha faz a limpeza do buffer
    VIDEO_BUFFER[row * MAX_COLS + col].c = ' ';

    spinlock_unlock(&spinlock_vbuffer);
}

/**
 * @brief Retorna true se o caractere é do tipo scape(\t, \n, \b)
 *
 * @param c
 * @return true
 * @return false
 */
static bool is_scape_char(const uint8_t c)
{
    bool result = false;

    if (c == __ESC_NEWLINE || c == __ESC_HTAB || c == __ESC_BACKSPACE)
    {
        result = true;
    }
    return result;
}

/**
 * @brief Faz o tratamento dos caracteres scape, fazendo os ajustes no
 * offset retornado
 *
 * @param c
 * @param offset
 * @return int
 */
static int parse_scape_char(const uint8_t c, int offset)
{
    int col = 0;
    int row = 0;

    if (c == __ESC_NEWLINE)
    {
        row = calcule_row(offset);
        row++;
        offset = calcule_offset(row, 0);
    }
    else if (c == __ESC_HTAB)
    {
        offset += screen_tab_size;
    }
    // backspace
    else if (c == __ESC_BACKSPACE)
    {
        offset--;
    }
    return offset;
}

static void refresh_screen_all(void)
{
    for (int row = frame.row_min; row < MAX_ROWS; row++)
        for (int col = 0; col < MAX_COLS; col++)
        {
            if (is_graphic_mode())
            {
                graphics_char(console_obj->gx, col * 8, row * 8, VIDEO_BUFFER[row * (MAX_COLS) + col].c);
            }
            else
            {
                write_screen_at(VIDEO_BUFFER[row * (MAX_COLS) + col].c, row, col);
            }
        }
}
/*  Teste  */
void refresh_screen_buffer(void)
{
    refresh_screen_all();
}

static void refresh_screen_line(int row)
{
    for (int col = 0; col < MAX_COLS; col++)
    {
        if (is_graphic_mode())
        {
            graphics_char(console_obj->gx, col * 8, row * 8, VIDEO_BUFFER[row * (MAX_COLS) + col].c);
        }
        else
        {
            write_screen_at(VIDEO_BUFFER[row * (MAX_COLS) + col].c, row, col);
        }
    }
}
void cls_current_line_at(int row, int pcol)
{
    spinlock_lock(&spinlock_vbuffer);

    for (int col = pcol; col < MAX_COLS; col++)
    {
        if (is_graphic_mode())
        {
            graphics_char(console_obj->gx, col * 8, row * 8, ' ');
        }
        else
        {
            VIDEO_BUFFER[row * MAX_COLS + col].c = ' ';
            write_screen_at(VIDEO_BUFFER[row * (MAX_COLS) + col].c, row, col);
        }
        VIDEO_BUFFER[row * MAX_COLS + col].c = ' ';
    }
    spinlock_unlock(&spinlock_vbuffer);

    set_cursor_pos(row, 0);
}
void cls_current_line(int row)
{
    spinlock_lock(&spinlock_vbuffer);

    for (int col = 0; col < MAX_COLS; col++)
    {
        if (is_graphic_mode())
        {
            graphics_char(console_obj->gx, col * 8, row * 8, ' ');
        }
        else
        {
            VIDEO_BUFFER[row * MAX_COLS + col].c = ' ';
            write_screen_at(VIDEO_BUFFER[row * (MAX_COLS) + col].c, row, col);
            set_cursor_pos(row, 0);
        }
        VIDEO_BUFFER[row * MAX_COLS + col].c = ' ';
    }
    spinlock_unlock(&spinlock_vbuffer);
}

void putchar(const uint8_t c)
{
    int offset = get_screen_cursor_pos();
    int row = calcule_row(offset);
    int col = calcule_col(offset);

    offset = print_char(c, row, col);
}

void puts(const void *message)
{
    int i = 0;
    while (((uint8_t *)message)[i] != 0)
    {
        putchar(((uint8_t *)message)[i++]);
    }
}

void clear_screen(void)
{
    /* Loop through video memory and write blank characters . */
    for (int row = 0; row < MAX_ROWS; row++)
    {
        for (int col = 0; col < MAX_COLS; col++)
        {
            write_screen_buffer(' ', row, col);
        }
    }

    if (is_graphic_mode())
    {
        console_clear_screen(console_obj);
    }
    else
    {
        // Transfere o conteúdo do buffer para a memória de vídeo
        refresh_screen_all();
        // Move the cursor back to the top left .
        set_screen_cursor_pos(calcule_offset(0, 0));
    }
}

int print_at(const char *message, int row, int col) /// Excluir
{
    int curr_offset = get_screen_cursor_pos();

    int offset = calcule_offset(row, col);

    if (!is_valid_screen_pos(row, col))
    {
        return -1;
    }
    spinlock_lock(&spinlock_vbuffer);
    /* Loop through message and print it */
    while ((*message) != '\0')
    {

        if (is_scape_char((*message)))
        {
            offset = parse_scape_char((*message), offset);
        }
        else
        {
            VIDEO_BUFFER[offset].c = (*message);
            offset++;
        }
        message++;
    }
    spinlock_unlock(&spinlock_vbuffer);

    refresh_screen_line(row);

    set_screen_cursor_pos(curr_offset);

    return offset;
}

/*
 * Esta rotina faz o verdadeiro trabalho de impressão na memória de vídeo
 */
int print_char(const uint8_t c, int row, int col)
{
    int offset = calcule_offset(row, col);

    if (!is_valid_screen_pos(row, col))
    {
        return -1;
    }

    if (is_scape_char(c))
    {
        offset = parse_scape_char(c, offset);
    }
    else
    {
        spinlock_lock(&spinlock_vbuffer);
        VIDEO_BUFFER[offset].c = c;
        spinlock_unlock(&spinlock_vbuffer);

        offset += 1;
    }

    /* Check if the offset is over screen size and scroll */
    if (offset >= MAX_ROWS * MAX_COLS)
    {
        spinlock_lock(&spinlock_vbuffer);

        for (int r = frame.row_min; r < MAX_ROWS; r++)
            for (int cl = 0; cl < MAX_COLS; cl++)
            {

                VIDEO_BUFFER[r * MAX_COLS + cl] = VIDEO_BUFFER[(r + 1) * MAX_COLS + cl];
            }

        /* Blank last line */
        int offset_line = calcule_offset(MAX_ROWS - 1, 0);
        for (int cl = 0; cl < MAX_COLS; cl++)
            VIDEO_BUFFER[offset_line + cl].c = ' ';
        //-------------------------------------------
        spinlock_unlock(&spinlock_vbuffer);

        offset -= MAX_COLS;
        refresh_screen_all();
    }

    refresh_screen_line(row);
    set_screen_cursor_pos(offset);
    return offset;
}

/* Copy bytes from one place to another . */
PRIVATE void copy_screen(ScreenWord_t *source, ScreenWord_t *dest, int no_pixel)
{
    int i;
    for (i = 0; i < no_pixel; i++)
    {
        dest[i].c = source[i].c;
        dest[i].cor = source[i].cor;
    }
}
void set_video_memory(void *pv)
{
    vidmem = (ScreenWord_t *)pv;
}
