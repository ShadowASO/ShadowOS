/*--------------------------------------------------------------------------
*  File name:  hpet.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 30-06-2021
*--------------------------------------------------------------------------
Este arquivo fonte possui diversas funções relacionadas ao
(HPET) - High Precision Event Timer.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "acpi.h"
#include "debug.h"
#include "stdio.h"
#include "string.h"
#include "screen.h"
#include "info.h"
#include "hpet.h"
#include "bits.h"
#include "sync/mutex.h"
#include "mm/fixmap.h"
#include "isr.h"
#include "interrupt.h"

// Ponteiro para a página vrtual que guarda a estrutura do HPET
hpet_reg_t *hpet;

hpet_reg_t *get_hpet_block(void)
{
    return hpet;
}

// Variável a ser utilizada como mutex
CREATE_SPINLOCK(spinlock_hpet);

// Rotina de teste
u64_t get_route_valid(void)
{
    // Pego o timer0
    hpet_timer_t *timer = hpet_timer(0);
    u64_t route = hpet_timer_route_cap(timer);
    return hpet_timer_route_cap(timer);
}

/* *********************************************************************
 *              Timer N Comparator Register
 * **********************************************************************/

/**
 * @brief  Inicia a geração de interrupções em todos os Timer's.
 * @note
 * @retval None
 */
void hpet_start(void)
{
    hpet_enable_global_timer();
}

/**
 * @brief  Suspende a geração de interrupções em todos os Timer's.
 * @note   O main couter continuar incrementando normalmente.
 * @retval
 */
void hpet_stop(void)
{
    hpet_disable_global_timer();
}

/*---------------------------------------------------------------------------------------*/
void setup_hpet(void)
{
    uint8_t ioapic_delivery = 1;

    /* Pego o endereço físico que foi guardado no hw_info pelo ACPI
    e faço o mapeamento para a memória virtual  */
    u64_t hpet_phys = hw_info.hpet[0].hpet_address;
    hpet = set_fixmap_nocache(FIX_HPET_BASE, hpet_phys);

    /*-------------------------------------------------------------------------*/
    /*ATENÇÃO: Apenas para teste. Escluir quando quiser. */
    /* É essencial que o timer global seja habilitado, ou o contrador principal
    //não será imcrementado. */
    // add_handler_irq(ISR_VECTOR_HPET_TIMER0, hpet_timer_handler2);
    // add_handler_irq(ISR_VECTOR_HPET_TIMER1, hpet_timer_handler8);
    /*--------------------------------------------------------------------------*/
    hpet_start();
    return;
}

/**
 * Configura o Timer indicado pelo timer_no, usando a
 * a frequência informada em "freq_hz".
 * O Hpet deve ser suspenso e depois startado. Com
 * essa providência, é possível configurar mais de um
 * Timer e inicializá-los juntos. Quando suspendemos o
 * Hpet, o incremento do main counter e os comparators
 * dos timers também ficam suspensos.
 * Usa o roteamento legacy replacement, onde o IO/APIC
 * recebe os seguintes IRQ de cada Timer:
 * Timer0=IRQ2;
 * Timer1=IRQ8

 * Ex.
    hpet_stop();
    hpet_timer_config_periodic(HPET_TIMER0, 100);
    hpet_start();

 */
bool hpet_timer_config_periodic(uint8_t timer_no, u64_t freq_hz)
{
    /* Verifica se o HPET foi suspenso, o que suspende todas as
    interrupções e o incremento do main counter. */

    if (is_global_timer_enabled())
    {
        WARN_ON("\nERROR - Desative o Timer Global no HPET.");
        return false;
    }

    hpet_timer_t *timer = &hpet->timers[timer_no];
    if (!hpet_timer_supports_periodic(timer))
    {
        WARN_ON("\nERROR - Timer [%d] no suporta periodic mode.", timer_no);
        return false;
    }
    /* Limpo o main counter. */
    hpet_main_counter_set(0);

    /* Configuro o timer. */
    hpet_timer_disable_int(timer);
    hpet_timer_enable_periodic(timer);
    // hpet_timer_irq_set(timer, 2);

    /* Ativa interrupções no timer.*/
    hpet_timer_enable_int(timer);

    /* Fixo o modo de interrupção.    */
    hpet_timer_level_set(timer);
    // hpet_timer_edge_set(timer);

    // u64_t ticks = hpet_convert_milliseconds_to_ticks(freq_ms);
    u64_t ticks = hpet_convert_freqhz_to_ticks(freq_hz);

    /* Atribuo o valor ao comparator do Timer. */
    hpet_timer_enable_periodic_accumulator(timer);
    hpet_timer_comparator_set(timer, ticks);

    kprintf("\nhpet_main_counter=%x", hpet_main_counter());
    kprintf("\nhpet_timer %d comparator=%x", timer_no, hpet_timer_comparator(timer));

    /* Uso o roteamento legacy replacement, onde o
    Timer0=IRQ2 e
    Timer1=IRQ8. */
    hpet_enable_route();

    /* Habilito as interrupções no timer. */
    hpet_timer_enable_int(timer);

    return true;
}

/**
 * @brief  Rotina que faz uma pausa pelo número de milissegundos informado em ms
 * @note
 * @param  ms:
 * @retval None
 */
void hpet_sleep_milli(u64_t time_ms)
{
    /* Garante que o HPET esteja ativo e seu main counter incrementando. */
    if (!is_global_timer_enabled())
    {
        WARN_ERROR("ERROR: Hpet disabled.");
    }

    hpet_sleep_nano(time_ms * 1000000);
}
