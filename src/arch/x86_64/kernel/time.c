/*--------------------------------------------------------------------------
*  File name:  time.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 21-08-2023
*--------------------------------------------------------------------------
Este arquivo fonte possui diversas funções relacionadas ao gerenciamento do
tempo.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "../include/time.h"
#include "ktypes.h"
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
#include "lapic.h"
#include "../drivers/graphic/console.h"
#include "scheduler.h"
#include "rtc.h"
#include "percpu.h"
#include "timer.h"
#include "time.h"

u64_t jiffies = 0;
u64_t wall_ticks = 0;

sys_timer_t sys_clock;

struct timespec xtime = {0, 0};
struct rtc_time rtc;

static int restoSemSinal(long a, int b)
{
    return (int)(a >= 0L ? a % b               // Positivo.
                         : (b + (a % b)) % b); // Negativo.
}

static long divisaoSemSinal(long a, int b)
{
    return a >= 0L
               ? a / b                           // Positivo.
               : (a / b) - (a % b == 0 ? 0 : 1); // Negativo.
}
/* Converte uma data no formato timestamp, contada em segundos e a converto
para o formato UTC. */
tm_t timestamp_to_utc(size_t timestampUnix)
{
    tm_t utc = {0};
    int segundo, minuto, hora, dia, mes, ano;

    // Passo 1.
    long minutosUnix = divisaoSemSinal(timestampUnix, 60);
    segundo = restoSemSinal(timestampUnix, 60);

    // Passo 2.
    long horasUnix = divisaoSemSinal(minutosUnix, 60);
    minuto = restoSemSinal(minutosUnix, 60);

    // Passo 3.
    long diasUnix = divisaoSemSinal(horasUnix, 24);
    hora = restoSemSinal(horasUnix, 24);

    // Passo 4.
    long ciclosDe400Anos = divisaoSemSinal(diasUnix, 146097);
    int diasEm400Anos = restoSemSinal(diasUnix, 146097);

    // Passo 5.
    if (diasEm400Anos >= 32 * 1461 + 789)
        diasEm400Anos++;
    if (diasEm400Anos >= 57 * 1461 + 789)
        diasEm400Anos++;
    if (diasEm400Anos >= 82 * 1461 + 789)
        diasEm400Anos++;

    // Passo 6.
    int ciclosDe4Anos = diasEm400Anos / 1461;
    int diasEm4Anos = diasEm400Anos % 1461;

    // Passo 7.
    if (diasEm4Anos >= 59)
        diasEm4Anos++;
    if (diasEm4Anos >= 425)
        diasEm4Anos++;
    if (diasEm4Anos >= 1157)
        diasEm4Anos++;

    // Passo 8.
    int anoEm4Anos = diasEm4Anos / 366;
    int diasNoAno = diasEm4Anos % 366;

    // Passo 9.
    ano = anoEm4Anos + ciclosDe4Anos * 4 + ciclosDe400Anos * 400 + 1970;

    // Passo 10.
    int tabelaDeMeses[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int contagemDeMeses = 0;
    while (diasNoAno >= tabelaDeMeses[contagemDeMeses])
    {
        diasNoAno -= tabelaDeMeses[contagemDeMeses];
        contagemDeMeses++;
    }
    mes = contagemDeMeses + 1;
    dia = diasNoAno + 1;

    utc.tm_year = ano;
    utc.tm_mon = mes;
    utc.tm_day = dia;
    utc.tm_hour = hora;
    utc.tm_min = minuto;
    utc.tm_sec = segundo;

    return utc;
}

/* Inicializa o relógio do systema, informando a frequência e o valor inicial
do contador. */
static inline void init_sys_clock(void)
{
    sys_clock.clk_frq = hpet_clk_periodo();
    sys_clock.ini_count = hpet_main_counter();
}

/* Atualiza o contador do sys_clock com o valor atual do timer
escolhido, no caso o hpet. */
static inline void update_sys_clock(void)
{
    sys_clock.cur_count = hpet_main_counter();
}

/* Devolve a quantidade de milissegundos transcorrido desde o início do relógio.
Esse valor é utilizado para alimentar jiffies e wall_ticks de forma mais precisa. */
// static inline u64_t sys_clock_elapse_milli(void)
static inline u64_t sys_clock_elapse_milli(void)
{
    update_sys_clock();
    sys_clock.cur_count = hpet_main_counter();
    return hpet_convert_ticks_to_milli(sys_clock.cur_count - sys_clock.ini_count);
}

static inline void update_wall_timer(u64_t ticks)
{
    xtime.tv_nsec += (ticks * TIME_NANOSEC) / TIMER_HZ;
    if (xtime.tv_nsec >= TIME_NANOSEC)
    {
        xtime.tv_sec += (xtime.tv_nsec / TIME_NANOSEC);
        xtime.tv_nsec = (xtime.tv_nsec % TIME_NANOSEC);
    }
}
static inline void update_times(void)
{
    u64_t ticks;
    ticks = jiffies - wall_ticks;
    if (ticks)
    {
        wall_ticks += ticks;
        update_wall_timer(ticks);
    }
}

/* Todas as interrupções do Timer selecionado(HPET, PIT) inicial
seu ciclo neste handler. */
void global_timer_handler(cpu_regs_t *tsk_contxt)
{
    // jiffies++;
    /* Colho diretamente do HPET o tempo decorrido, por ser mais preciso e aparentemente não
    interfere da performance. */
    jiffies = sys_clock_elapse_milli();
    update_times();
    run_timers();
}

/* Inicia as rotinas de medição do tempo do sistema. */
void time_init(void)
{
    /* Faço a inicialização das estruturas do HPET.*/
    setup_hpet();

    /* Paro o main counter do HPET. */
    hpet_stop();

    /* Zero o main counter e cConfiguro o HPET para o modo periódico.*/
    hpet_timer_config_periodic(HPET_TIMER1, TIMER_HZ);

    /* Pega a hora do RTC/CMOS. Posteriormente, converte em segundos. */
    rtc = get_rtc_time();

    /* Inicia o main counter do HPET. */
    hpet_start();

    /* Inicializo o sys_clock baseado no main counter do HPET. */
    init_sys_clock();

    /* wall time. */
    xtime.tv_nsec = 0;
    xtime.tv_sec = mktime(rtc.year, rtc.mth, rtc.day, rtc.hr, rtc.min, rtc.sec);

    /*-----------------------------------------*/

    /* Relógio do terminal.                     */
    // load_new_timer(&clock_timer, show_clock, ((mm_addr_t)&clock_timer), (get_jiffies() + TIME_MILISEC));
    /*------------------------------------------*/

    /* Vinculo o handler global do timer e ativo as interrupções do timer. */
    add_handler_irq(ISR_VECTOR_HPET_TIMER1, global_timer_handler);
    irq_umask(ISR_VECTOR_HPET_TIMER1, cpu_id());
}
