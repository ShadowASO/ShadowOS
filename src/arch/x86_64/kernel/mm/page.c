/*--------------------------------------------------------------------------
*  File name:  pma.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 01-08-2020
*  Revisão: 21-05-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a alocação de
páginas físicas.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "string.h"
#include "stdio.h"
#include "debug.h"
#include "mm/page_alloc.h"
#include "mm/bitmap.h"
#include "mm/page-flags.h"
#include "mm/page.h"
#include "mm/pgtable_types.h"
#include "mm/mm.h"
#include "mm/bootmem.h"
