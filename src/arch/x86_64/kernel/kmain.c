/*--------------------------------------------------------------------------
 *  File name:  kmain.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 22-07-2020
 *--------------------------------------------------------------------------
 *  Rotina principal de entrada no kernel.
 *--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "debug.h"
#include "lapic.h"
#include "info.h"
#include "mm/mm_heap.h"
#include "smp.h"
#include "../drivers/graphic/vesa.h"
#include "scheduler.h"
#include "mm/vmalloc.h"
#include "../drivers/IO/mouse.h"
#include "time.h"
#include "../drivers/time/tsc.h"
#include "syscall/syscalls.h"

mm_addr_t _stack_rsp = 0;
mm_addr_t _stack_rbp = 0;

void kmain(uint32_t __attribute__((unused)) phy_maps);

/**
 * @brief Estrutura que guarda as informações extraídas do ACPI e relativas
 * aos recursos do sistema.
 */
hw_info_t hw_info;

/**
 * @brief Rotina de entrada do kernel e onde fazemos as primeiras configura-
 * ções do sistema e estruturas de dados.
 *
 * @param __attribute__
 */
void kmain(uint32_t __attribute__((unused)) phy_maps)
{
    local_irq_disable();

    hw_info.mboot_addr = phy_maps;

    /* Rotina que inicializa o gerenciamento da memória.*/
    setup_memory();

    // Todas as rotinas essenciais devem ser chamadas até aqui, porque após
    // executar scheduler_bsp, nenhum código posterior será executado

    /* ATENÇÃO: A partir daqui NÃO podemos mais utilizar o alloc_bootmem.
    Todas as alocações de memória devem ser feitas utilizando:
    kalloc_frames("zona id", 0) */

    vmalloc_init();

    setup_heap();

    setup_acpi();

    setup_apic();

    ioapic_ini();

    // setup_hpet();
    time_init();

    apic_ini();

    setup_keyboard();

    // init_mouse();

    setup_vesa();

    smp_init();

    // time_init();

    // clear_screen();
    // hpet_sleep_milli_sec(50);
    setup_syscall();

    local_irq_enable();

    scheduler_bsp();

    __busy_wait();

    DBG_PAUSE("Finalizando kmain");
}
