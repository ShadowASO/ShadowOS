/*--------------------------------------------------------------------------
*  File name:  read.h
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 10-10-2022
*--------------------------------------------------------------------------
Este header reune estruturas para o Page Memory Allocator
--------------------------------------------------------------------------*/
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include "kernel.h"
#include "syscall/syscalls.h"

static inline uint8_t getc(void)
{
    return syscall_exec(__NR_read, 0, 0, 0, 0, 0);
}