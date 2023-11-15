/*--------------------------------------------------------------------------
 *  File name:  itoa.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 21-07-2020
 *--------------------------------------------------------------------------*/
// Rotinas para calcular o número de caracteres em uma strings.
//--------------------------------------------------------------------------

#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"
#include "ctype.h"
#include "stdlib.h"
#include "string.h"

u64_t atou(const char *s)
{
    unsigned int i = 0;
    while (isdigit(*s))
        i = i * 10 + (*s++ - '0');
    return i;
}

u64_t stoi(const char *s) // the message and then the line #
{
    u64_t i;
    i = 0;
    while (*s >= '0' && *s <= '9')
    {
        i = i * 10 + (*s - '0');
        s++;
    }
    return i;
}

/**
 * @brief Converte um inteiro para uma string na base indicada.
 *
 * @param num
 * @param str
 * @param base
 * @return uint8_t*
 */
char *itoa(long long int num, char *str, int base)
{
    int i = 0;
    bool isNegative = false;

    /* Handle 0 explicitely, otherwise empty string is printed for 0 */
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // In standard itoa(), negative numbers are handled only with
    // base 10. Otherwise numbers are considered unsigned.
    // if (num < 0 && base == 10)
    // Converto todos para positivo
    if (num < 0)
    {
        isNegative = true;
        num = -num;
    }

    // Process individual digits
    while (num != 0)
    {
        int mod = num % base;
        str[i++] = (mod > 9) ? (mod - 10) + 'a' : mod + '0';
        num = num / base;
    }

    // If number is negative, append '-'
    if (isNegative)
        str[i++] = '-';

    str[i] = '\0'; // Append string terminator

    // Reverse the string
    reverse(str, i);
    return str;
}

unsigned long long int strtoull(const char *ptr, char **end, int base)
{
    unsigned long long ret = 0;

    if (base > 36)
        goto out;

    while (*ptr)
    {
        int digit;

        if (*ptr >= '0' && *ptr <= '9' && *ptr < '0' + base)
            digit = *ptr - '0';
        else if (*ptr >= 'A' && *ptr < 'A' + base - 10)
            digit = *ptr - 'A' + 10;
        else if (*ptr >= 'a' && *ptr < 'a' + base - 10)
            digit = *ptr - 'a' + 10;
        else
            break;

        ret *= base;
        ret += digit;
        ptr++;
    }

out:
    if (end)
        *end = (char *)ptr;

    return ret;
}