/*--------------------------------------------------------------------------
*  File name:  string_parser.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 06-03-2020
*--------------------------------------------------------------------------
Este arquivo de possui rotinas para realizar o parse de uma string, utilizando
os tokens de ANSI printf.
--------------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdbool.h>
#include "screen.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#define TMPBUF_SIZE 64
#define BUFFER_SIZE 256
// static char buffer[BUFFER_SIZE] = {'.'};

/**
 * @brief Rotina para transformar dígitos numéricos individuais em caracteres.
 *
 * @param digit
 * @return char
 */
static inline char digit_to_char(unsigned int digit)
{
    if (digit < 16)
    {
        return "0123456789ABCDEF"[digit];
    }
    else
    {
        return '\0';
    }
}

static inline int bin_to_char(char *pt, u64_t x)
{
    char str[64 + 1] = {'\n'};
    u64_t n = x;
    int i = 0;
    int nibble = 0;

    while (n)
    {
        if (nibble == 4)
        {
            nibble = 0;
            str[i++] = '.';
        }
        str[i++] = ((n & 1) ? '1' : '0');
        n >>= 1;
        nibble++;
    }
    str[i] = '\0';
    reverse(str, i);
    strncpy(pt, str, i);
    return i;
}
/**
 * @brief Converte o endereço em um ponteiro para uma string.
 *
 * @param pt
 * @param ptr
 * @return int
 */
static inline int pointer_to_string(char *pt, uintptr_t ptr)
{
    char buf_tmp[TMPBUF_SIZE] = {'.'};
    int i = 0;

    while (ptr)
    {
        buf_tmp[i++] = digit_to_char(ptr & 0xF);
        ptr >>= 4;
    }
    buf_tmp[i++] = 'x';
    buf_tmp[i++] = '0';

    reverse(buf_tmp, i);
    strncpy(pt, buf_tmp, i);

    return i;
}

/**
 * @brief Converte um número inteiro para uma string, na base desejada.
 *
 * @param pt
 * @param num
 * @param sign
 * @param base
 * @return int
 */
static inline int number_to_char(char *pt, u64_t num, bool sign, unsigned int base)
{
    char buf_tmp[TMPBUF_SIZE] = {'.'};
    int i = 0;

    int64_t n = (int64_t)num;

    if (num == 0)
    {
        buf_tmp[i++] = '0';
    }
    else if (sign)
    {

        if (n < 0)
        {
            buf_tmp[i++] = '-';
            n = -n;
        }
        while (n != 0)
        {
            buf_tmp[i++] = digit_to_char(n % base);
            n = n / base;
        }
    }

    else
    {
        while (n != 0)
        {
            buf_tmp[i++] = digit_to_char(n % base);
            n = n / base;
        }
    }
    if (base == 16)
    {
        buf_tmp[i++] = 'x';
        buf_tmp[i++] = '0';
    }

    reverse(buf_tmp, i);
    strncpy(pt, buf_tmp, i);

    return i;
}
/**
 * @brief Rotina para a realização de parses em strings, utilizando os tokens do
 * printf. Devolve um ponteiro char * para um buffer temporário.
 *
 * @param fmt
 * @param params
 * @return char*
 */
static inline char *__parser_str(char *buf, const char *fmt, va_list params)
{
    // char *p = buffer;
    char *p = buf;
    size_t i = 0;
    while (*fmt)
    {
        if (*fmt == '%')
        {
            fmt++;

            if (*fmt == 's')
            {
                const char *s = va_arg(params, const char *);
                if (s == NULL)
                    s = "(null)";

                strcpy(p, s);
                i += strlen(s);
            }
            else if (*fmt == 'c')
            {
                char c = (char)va_arg(params, int);
                p[i++] = c;
            }
            else if (*fmt == 'd')
            {
                i = i + number_to_char((p + i), va_arg(params, int), true, 10);
            }
            else if (*fmt == 'o')
            {
                i = i + number_to_char((p + i), va_arg(params, unsigned int), false, 8);
            }
            else if (*fmt == 'u')
            {
                i = i + number_to_char((p + i), va_arg(params, unsigned int), false, 10);
            }
            else if (*fmt == 'x')
            {
                i = i + number_to_char((p + i), va_arg(params, unsigned int), false, 16);
            }
            else if (*fmt == 'b')
            {
                // i = i + number_to_char((p + i), va_arg(params, int), false, 2);
                i = i + bin_to_char((p + i), va_arg(params, int));
            }
            else if (*fmt == 'p')
            {
                i = i + pointer_to_string((p + i), (uintptr_t)va_arg(params, void *));
            }
            else if (*fmt == '%')
            {
                p[i] = (*fmt);
                i++;
            }
            else
            {
                p[i] = (*fmt);
                i++;
            }
        }
        else
        {
            p[i] = (*fmt);
            i++;
        }

        fmt++;
    }
    p[i] = '\0';

    // return buffer;
    return p;
}

char *parser_token(char *buf, const char *fmt, ...)
{
    va_list params;
    va_start(params, fmt);
    char *p = __parser_str(buf, fmt, params);
    va_end(params);
    return p;
}
