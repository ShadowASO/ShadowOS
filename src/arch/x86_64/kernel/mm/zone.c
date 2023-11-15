/*--------------------------------------------------------------------------
*  File name:  zone.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 12-06-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a alocação de
páginas físicas.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "mm/zone.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/pgtable_types.h"

char *zone_name[] = {"ZONA DMA", "ZONA NORMAL", "ZONA HIGH"};

char *get_zone_name(u8_t zone_id)
{
    return zone_name[zone_id];
}