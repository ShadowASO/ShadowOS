/*--------------------------------------------------------------------------
*  File name:  tsc.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 27-08-2023
*--------------------------------------------------------------------------
Este arquivo fonte possui diversas funções relacionadas ao gerenciamento do
tempo. Mas o TSC não deve ser utilizado, pois sua frequência varia muito.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "tsc.h"
#include "ktypes.h"
#include "debug.h"
#include "stdio.h"
#include "string.h"
#include "hpet.h"
#include "percpu.h"
#include "time.h"

/*
 * calibrate_tsc() calibrates the processor TSC in a very simple way, comparing
 * it to the HPET timer of known frequency.
 */

#define TICK_COUNT 100000000

/* Devolve a quantidade de tick dados pelo TSC em um millisecond. */
u64_t hpet_calibrate_tsc(void)
{
    int tsc_start = 0;
    int hpet_start = 0;
    int tsc_now = 0;
    int hpet_now = 0;
    unsigned long flags = 0;

    u64_t hpet_period = hpet_clk_periodo();

    // local_irq_save(flags);
    local_irq_disable();

    hpet_start = hpet_main_counter();
    tsc_start = tsc_counter_read();

    do
    {
        local_irq_disable();

        hpet_now = hpet_main_counter();
        sync_core();
        tsc_now = tsc_counter_read();
        //  local_irq_restore(flags);
    } while ((tsc_now - tsc_start) < TICK_COUNT &&
             (hpet_now - hpet_start) < TICK_COUNT);

    /* Devolve a freqência do TSC por milissecond. */
    return (tsc_now - tsc_start) * 1000000000L / ((hpet_now - hpet_start) * hpet_period / 1000);
}
/* Devolve o número de femptoseconds de cada cliclis no TSC. */
u64_t TSC_periodo(void)
{
    int tsc_start = 0;
    int hpet_start = 0;
    int tsc_now = 0;
    int hpet_now = 0;
    u64_t tsc_tick_fs = 0;
    u64_t tmp = 0;

    u64_t hpet_period = hpet_clk_periodo();

    local_irq_disable();

    __sync_lfence();
    __sync_mfence();
    // hpet_start = hpet_main_counter();
    //  tsc_start = tsc_counter_read();
    tsc_start = tsc_read();
    hpet_start = hpet_main_counter();

    do
    {
        // hpet_now = hpet_main_counter();
        tsc_now = tsc_read();
        hpet_now = hpet_main_counter();
    } while ((tsc_now - tsc_start) < TICK_COUNT &&
             (hpet_now - hpet_start) < TICK_COUNT);

    tmp = ((hpet_now - hpet_start) * hpet_period);
    tsc_tick_fs = tmp / (tsc_now - tsc_start);

    /* Devolve a freqência do TSC por milissecond. */
    // return (tsc_now - tsc_start) *
    //        1000000000L / ((hpet_now - hpet_start) * hpet_period / 1000);

    local_irq_enable();
    return tsc_tick_fs;
}
