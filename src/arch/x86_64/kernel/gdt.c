/*--------------------------------------------------------------------------
*  File name:  gdt.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 01-12-2022
*--------------------------------------------------------------------------
Este header reune estruturas
--------------------------------------------------------------------------*/

#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"
#include "gdt.h"
#include "stdio.h"
#include "bits.h"
#include "debug.h"

void create_system_descriptor(virt_addr_t gdtp, u16_t offset, mm_addr_t segment, u32_t limit, u8_t type, u8_t flags)
{
    gdtd_sys_t *d = incptr(gdtp, offset);

    /* Limite do segmento. */
    d->limit0 = BF_GET(limit, 0, 16);
    d->flags = BF_GET(limit, 16, 4); /* 4 bits do campo compõem o limits. */

    kprintf("segment=%x - limit=%d - type=%x - flags=%x", segment, limit, type, flags);

    /* Endereço do segmento.*/
    d->base0 = BF_GET(segment, 0, 16);
    d->base16 = BF_GET(segment, 16, 8);
    d->base24 = BF_GET(segment, 24, 8);
    d->base32 = BF_GET(segment, 32, 32);

    /* Atributos. */
    d->type = type;
    d->flags = (d->flags | flags); /* Concateno com o flag G. */
}
void create_code_descriptor(virt_addr_t gdtp, u16_t offset, mm_addr_t segment, u32_t limit, u8_t type, u8_t flags)
{
    gdtd_sys_t *d = incptr(gdtp, offset);

    /* Limite do segmento. */
    d->limit0 = BF_GET(limit, 0, 16);
    d->flags = BF_GET(limit, 16, 4); /* 4 bits do campo compõem o limits. */

    /* Endereço do segmento.*/
    d->base0 = BF_GET(segment, 0, 16);
    d->base16 = BF_GET(segment, 16, 8);
    d->base24 = BF_GET(segment, 24, 8);
    // d->base32 = BF_GET(segment, 32, 32);

    /* Atributos. */
    d->type = type;
    d->flags = (d->flags | flags); /* Concateno com o flag G. */
}