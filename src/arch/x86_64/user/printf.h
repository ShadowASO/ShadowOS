/*--------------------------------------------------------------------------
*  File name:  printf.h
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 07-10-2022
*--------------------------------------------------------------------------
Este header reune estruturas para o Page Memory Allocator
--------------------------------------------------------------------------*/
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#define PRINTF_MAX_SIZE 256

size_t printf(char *fmt, ...);