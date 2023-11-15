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

#include "mm/mm.h"
#include "mm/page.h"
#include "../info.h"
#include "multiboot2.h"
#include "mm/page_alloc.h"
#include "mm/mm_types.h"
#include "mm/kmalloc.h"
#include "mm/bitmap.h"
#include "string.h"
#include "debug.h"
#include "mm/bootmem.h"

mm_struct_t init_mm;

/**
 * @brief Principal rotina para a cnfiguração dos controles da memória
 * física e estruturação do vetor de pages. É o coração do gerenciamen-
 * to da memória física.
 *
 */
void setup_memory(void)
{
    kprintf("\n(*)%s(%d) - Fazendo o setup da memory.", __FUNCTION__, __LINE__);

    init_bootmem();

    kprintf("\n(*)%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}
