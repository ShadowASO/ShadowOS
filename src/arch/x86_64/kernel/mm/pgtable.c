/*--------------------------------------------------------------------------
*  File name:  pgtable.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 26-06-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a alocação de
páginas físicas.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "string.h"
#include "mm/mm_types.h"
#include "mm/pgtable.h"
#include "mm/page.h"
#include "mm/page-flags.h"
#include "mm/mm.h"

static inline phys_addr_t _pt4_alloc(void)
{
    u64_t frame = alloc_frames(GFP_ZONE_NORMAL, 0);
    memset(phys_to_virt(frame), 0, PAGE_SIZE);
    return frame;
}
phys_addr_t pt4_alloc(mm_struct_t *mm)
{
    phys_addr_t frame = _pt4_alloc();
    mm->pml4 = phys_to_virt(frame);
    return frame;
}
