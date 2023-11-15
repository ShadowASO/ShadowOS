/*--------------------------------------------------------------------------
*  File name:  printf.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 07-10-2022
*--------------------------------------------------------------------------
Este header reune estruturas para o Page Memory Allocator
--------------------------------------------------------------------------*/

#include <stdarg.h>
#include <stdbool.h>
#include "../user/printf.h"
#include "kernel.h"
#include "syscall/syscalls.h"

size_t printf(char *fmt, ...)
{
    va_list args;
    int i;
    char pbuf[PRINTF_MAX_SIZE];

    va_start(args, fmt);

    i = vsnprintf(pbuf, PRINTF_MAX_SIZE, fmt, args);

    va_end(args);

    return syscall_exec(__NR_write, 0, (mm_addr_t)pbuf, i, 0, 0);
}