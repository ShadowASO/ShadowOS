/*--------------------------------------------------------------------------
 *  File name:  smbios.h
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 24-02-2023
 *--------------------------------------------------------------------------
 *  Rotinas para inserir um caractere em uma strings, com ajuste no tamanho.
 *--------------------------------------------------------------------------*/
#pragma once

#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"
#include "ktypes.h"
#include "stdio.h"
#include "../mm/mm.h"

#define SMBIOS_PHYS_START 0xF0000
#define SMBIOS_PHYS_PEND 0x100000

struct smbios_header
{
    u8_t type;
    u8_t length;
    u16_t handle;
} __attribute__((packed));

extern phys_addr_t smbios_phys_address;

void smbios_init(void);

static inline bool smbios_checksum(virt_addr_t base, u8_t len)
{
    u8_t *mem = base;
    u8_t sum = 0;
    for (size_t i = 0; i < len; i++)
    {
        sum += mem[i];
    }
    return sum == 0;
}
static inline bool find_smbios_log(void)
{
    phys_addr_t ini = (phys_addr_t)SMBIOS_PHYS_START;
    phys_addr_t pend = (phys_addr_t)SMBIOS_PHYS_PEND;
    char *mem = phys_to_virt(ini);

    while (ini < pend)
    {
        if (mem[0] == '_' && mem[1] == 'S' && mem[2] == 'M' && mem[3] == '_')
        {
            if (smbios_checksum(mem, mem[5]))
            {
                smbios_phys_address = (ini);
                kprintf("\nSMBIOS=%x", smbios_phys_address);
                return true;
            }
        }

        if (mem == phys_to_virt(SMBIOS_PHYS_PEND))
        {
            kprintf("No SMBIOS found!");
        }

        ini += 16;
        mem += 16;
    }

    return false;
}
