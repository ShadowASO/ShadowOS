/*--------------------------------------------------------------------------
 *  File name:  scheduler.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 10-02-2023
 *--------------------------------------------------------------------------
 *  Rotina de gerenciamento do scheduler.
 *--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"
#include "sync/atomic.h"
#include "task.h"
#include "interrupt.h"
#include "shell.h"
#include "hpet.h"
#include "smp.h"
#include "percpu.h"
#include "runq.h"
#include "lapic.h"
#include "../drivers/graphic/console.h"
#include "scheduler.h"
#include "tss.h"
#include "mm/vmalloc.h"

static atomic32_t schedulers_waiting;
CREATE_SPINLOCK(spinlock_task);
extern virt_addr_t pidTable;

static void recalc_priority(task_t *t)
{
    u8_t cpu = get_task_cpu(t);
    struct runq *rq = percpu_table[cpu].run_queue;
    prio_array_t *array = &rq->arrays;

    __spin_lock(&rq->lock.key);

    // dequeue_task(t, array);
    runq_remove(t);
    if (t->priority < LOW_PRIO)
    {
        t->priority++;
    }
    else
    {
        t->priority = t->static_priority;
    }
    // enqueue_task(t, array);
    runq_add(t, cpu);

    __spin_unlock(&rq->lock.key);
}

/* É possível fazer muita coisa em C, sem a necessidade de utilizar Assembly. */
static void switch_to(task_t *task_curr, task_t *task_next)
{
    u8_t cpu = cpu_id();
    tss_t *tss = percepu_get_tss(cpu);
    tss->rsp0 = (mm_addr_t)task_next->rsp0;

    // tss->rsp0 = (mm_addr_t)task_next->stack_rsp;
}

/**
 * @brief  Rotina que faz a mudança de contexto dos task's
 * em execução.// preempt_enable();
 * @note
 * @retval None
 */
void scheduler(void)
{
    u8_t cpu = cpu_id();
    struct task *next = NULL;

    /*  Disable preemption */
    preempt_disable();

    /* Ativo o rescheduler no CORE atual. Isso permite que bloqueemos o rescheduler num
    CORE pelo tempo desejado. Ele será reativado diretametne ou mediante sched_yield(). */
    percpu_reschedule_enable();

    // Update scheduler statistics for the previous task
    percpu_current()->sched.vruntime++;

    /* Se o task atualmente em execução tiver alguma atividade útil e não apenas um
    loop infinito, como no idle task, o reinserimos na rbtree. Isso é necessário porque
    todas as fezes que invocamos runq_next(), ela devolve um task para excução, mas o
    exclui da fila de execução. O task que está em execução foi excluído e é aqui que
    decidimos se ele deve continuar sendo executado.
    */
    if (!is_task_idle(percpu_current()))
    {
        // recalc_priority(percpu_current());
        runq_requeue(percpu_current(), cpu);
    }

    // struct task *next = find_next_task();
    if (is_task_idle(percpu_current()))
    {
        //  runq_balance_pull(); //Sempre gera erro na inicialização. DEscobrir motivo.
        //  next = runq_next();
    }

    next = runq_next(cpu);

    /* Altero o state do task atual e do próximo. */
    switch_to(percpu_current(), next);

    __switch_to(next);
}

/*
Handler executado a cada interrupção do Lapic Timer. O switch do task somente ocorre
quando atigida a quantidade de "slice" determinada. Além disso, exige que a preempção
esteja ativada, para evitar a troca de contexto dentro de uma área crítica.
*/
static void apic_timer_handler(cpu_regs_t *tsk_contxt)
{
    struct task *t = percpu_current();
    t->sched.num_slices++;

    /* Preempt a task after it's ran for 5 time slices. */
    if (t->sched.num_slices >= TASK_SLICES_MAX)
    {
        /* Se a interrupção não tiver acontecido dentro de uma área crítica, faz o switch. */
        if (is_percpu_preempt() && is_percpu_reschedule())
        {
            t->sched.num_slices = 0;
            sched_yield();
        }
    }
    return;
}
/*    Atribui o primeiro task, cuja estrutura é estática, para representar  o
    código que está executando no início da execução.*/
static void sched_setup(void)
{
    /* Crio uma tabela para receber o endereço de cada task, indexado pelo pid.
    A tabela comporta apenas 32.768 endereços tasks. */
    pidTable = vmalloc(PID_MAX * 8);
    memset(pidTable, 0, PID_MAX);

    /* Atribuo o handler do ISR que fará o tratamento das interrupções do Apic Timer. */
    add_handler_irq(ISR_VECTOR_TIMER, apic_timer_handler);
}
static inline void wait_for_schedulers(void)
{
    atomic_inc_read32(&schedulers_waiting);
    while (atomic_read32(&schedulers_waiting) < smp_nr_cpus())
        ;

    if (is_bsp())
        hpet_sleep_milli(500);

    spinlock_lock(&spinlock_task);

    kprintf("\n(*)sched: Starting scheduler for CPU %d...\n", smp_cpu_id());

    spinlock_unlock(&spinlock_task);
}
/* Em contrução */
static void init_func(void)
{
    /* A primeira task do BSP será o shell. */
    // struct task *t1 = task_init(percpu_current(), shell_ini, THREAD_KERNEL | MODE_KERNEL, NULL);

    struct task *t1 = task_fork(0, shell_ini, THREAD_KERNEL | MODE_KERNEL, NULL);
    task_wakeup(t1);

    // wait_for_schedulers();

    // init_lapic_timer(SCHEDULER_SLICE_TIME);

    // debug_pause("p1");

    // WARN_ON("CORE=[ %d ]: init_task=%p - shell task=%p", cpu, init_task, t1);

    // preempt_enable();
    // percpu_reschedule_set(true);

    // sched_yield();
    while (true)
    {
        __PAUSE__();
        __HLT__();
    }
}
/**
 * @brief  Cria a estrutura e configuração do scheduler, fazendo
 * inicialização do vetor de task_lists com o respectivo header.
 * Sem essa inicialização, as rotinas seguintes apresentarão erro.
 * @note
 * @retval None
 */
void scheduler_bsp(void)
{
    kprintf("\n\nLinha: %d - SCHEDULER_BSP: Inicializando o scheduler.\n", __LINE__);

    u8_t cpu = cpu_id();
    struct task *init_task = get_init_task(cpu);

    /* Atribui o handler "apic_timer_handler" para tratar as interrupções de relógio
    geradas pelo LAPIC TIMER de cada CORE. Sem esse handler, a troca de contexto não
    ocorre.
    */
    sched_setup();
    /*------------------------------------------------------------------------------*/

    /* O task "init_task" serve apenas para o caso de inexistirem outros tasks para
    execução do scheduler. Esse task executa apenas um laço infinito, enquanto não
    há qualquer task para ser executada. Registramos esse task como o corrente no
    percpu para uso na primeira execução do scheduler().*/
    percpu_current_set(init_task);

    /* Cria uma run queue e atribui "init_task" como o (idle_task) a ser usado quando
    a fila de tasks estiver vazia.
    ATENÇÃO: Esta rotina deve ser chamada uma ÚNICA vez para cada CORE.
    */
    runq_init(init_task);
    /*------------------------------------------------------------------------------*/

    /* A primeira task do BSP será o shell. */
    // struct task *t1 = task_fork(init_task, shell_ini, THREAD_KERNEL | MODE_KERNEL, NULL);
    struct task *t1 = task_fork(init_task, shell_ini, MODE_USER, NULL);
    task_wakeup(t1);

    /* Aguarda que todos os COREś tenham iniciado antes de prosseguir. */
    wait_for_schedulers();

    /* Ativa o lapic timer do corrente CORE. */
    init_lapic_timer(SCHEDULER_SLICE_TIME);

    /* Desativo a preempção para esta CPU e volto a reativar no scheduler(). Isso
    evita uma condição de corrida. Não há risco de permanecer desativada, pois a
    primeira execução do scheduler() em cada CORE é forçada por sched_yield().*/
    // percpu_reschedule_set(false);
    percpu_reschedule_disable();

    // kprintf("\nCORE=[ %d ]: PID [ %d ] init_task=%p - shell task=%p", cpu, t1->pid, init_task, t1);
    //  debug_pause("bsp");

    sched_yield();
}
/* Ativer o scheduler para cada CORE, criando um init_task, uma fila de execu-
ção vazia, ativa o LAPIC TIMER de cada core. Como a fila está vazia, o scheduler
ira utilizar o "init_task", que é um idle task, pois executa apenas um loop in-
finito("idle_func")*/
void scheduler_ap(void)
{
    kprintf("\nLinha: %d - SCHEDULER_AP: Inicializando o scheduler.", __LINE__);

    u8_t cpu = cpu_id();
    struct task *init_task = get_init_task(cpu);

    /* O task "init_task" serve apenas para o caso de inexistirem outros tasks para
    execução do scheduler. Esse task executa apenas um laço infinito, enquanto não
    há qualquer task para ser executada. Registramos esse task como o corrente no
    percpu para uso na primeira execução do scheduler().*/
    percpu_current_set(init_task);

    /* Cria uma run queue e atribui "init_task" como o (idle_task) a ser usado quando
    a fila de tasks estiver vazia.
    ATENÇÃO: Esta rotina deve ser chamada uma ÚNICA vez para cada CORE.
    */
    runq_init(init_task);
    /*------------------------------------------------------------------------------*/

    /* Aguarda que todos os COREś tenham iniciado antes de prosseguir. */
    wait_for_schedulers();

    /* Ativa o lapic timer do corrente CORE. */
    init_lapic_timer(SCHEDULER_SLICE_TIME);

    /* Desativo a preempção para esta CPU e volto a reativar no scheduler(). Isso
    evita uma condição de corrida. Não há risco de permanecer desativada, pois a
    primeira execução do scheduler() em cada CORE é forçada por sched_yield().*/
    // percpu_reschedule_set(false);
    percpu_reschedule_disable();

    // kprintf("\nCORE=[ %d ]: PID [ %d ] init_task=%p", cpu, init_task->pid, init_task);

    sched_yield();
}

void task_wakeup(struct task *t)
{
    /* Desativa a preempção para esta CPU. */
    preempt_disable();

    t->state = eSTATE_RUNNING;
    sched_add(t);

    /* Ativo a preempção para esta CPU. */
    preempt_enable();
}
void sched_add(struct task *t)
{
    /* Desativa a preempção para esta CPU. */
    preempt_disable();

    runq_add(t, get_task_cpu(t));

    /* Ativo a preempção para esta CPU. */
    preempt_enable();
}

/* Chamada quando um task encerra sua execução eentrega voluntariamente
 * o controle da CPU.*/
void sched_yield(void)
{
    // preempt_disable();
    // percpu_current()->state |= eSTATE_RUNNING;
    // preempt_enable();

    scheduler();
}

cpuid_t sched_execve(task_t *task)
{
    preempt_disable();

    struct runq *rq_tmp = NULL;
    struct percpu *pcpu = NULL;
    cpuid_t cpu = 0;
    size_t min = 0;
    size_t tmp = 0;

    pcpu = percpu_by_core(cpu);
    rq_tmp = pcpu->run_queue;
    min = rq_tmp->nr_threads;

    for (size_t i = cpu; i < smp_nr_cpus(); i++)
    {
        pcpu = percpu_by_core(i);
        if (pcpu->cpu_id == i)
        {
            rq_tmp = pcpu->run_queue;
            tmp = rq_tmp->nr_threads;
            // kprintf("\nCORE[%d]: percpu=%p- runq=%p -  threads=%d", i, pcpu, rq_tmp, tmp);
            if (tmp < min)
            {
                cpu = i;
                min = tmp;
            }
        }
    }

    pcpu = percpu_by_core(cpu);
    rq_tmp = pcpu->run_queue;

    /* Coloco em stado de execução*/
    task->state = eSTATE_RUNNING;

    kprintf("\n(*)CORE[ %d ] - pcpu=%p - rq_tmp=%p ", cpu, pcpu, rq_tmp);

    runq_add(task, cpu);

    preempt_enable();

    return cpu;
}
void show_cpu(void)
{
    char *cpu_str = "[%d]";
    char *str = NULL;
    char buf[10] = {0};

    int row = 0;
    int col = 0;

    uint32_t cpu_id = smp_cpu_id();
    u64_t pid = 0;
    bool blink = true;

    console_t *cpu_console = NULL;
    if (is_graphic_mode())
    {
        cpu_console = console_create(graphic_obj);
    }

    while (true)
    {
        preempt_disable();

        cpu_id = smp_cpu_id();
        pid = percpu_current()->pid;
        col = 75 - (cpu_id * 4);

        str = parser_token(buf, cpu_str, cpu_id);

        if (blink)
            str = "[ ]";

        if (is_graphic_mode())
        {
            console_putstring_at(cpu_console, col, row, str);
        }
        else
        {
            print_at(str, row, col);
        }
        blink = (blink) ? false : true;

        preempt_enable();
        // hpet_sleep_milli(50);

        sched_yield();
    }
    console_delete(cpu_console);
}

void dump_task(task_t *task)
{
    kprintf("\n--------------------------------------------");
    kprintf("\nRSP=%x", __read_reg_rsp());
    kprintf("\nIST3=%x", __tss_ist_read(__get_curr_tss(), TSS_IST3));

    kprintf("\nid=%d", task->pid);
    kprintf("\nstack_rbp=%x", task->stack_rbp);
    kprintf("\nstack_rsp=%x", task->stack_rsp);
    // kprintf("\nist_rbp=%x", task->ist_rbp);
    // kprintf("\nist_rsp=%x", task->ist_rsp);
}

void dump_tss()
{
    static bool on = true;
    char a = 0;

    if (on)
    {

        kprintf("\n--------------------------------------------");
        // kprintf("\n__get_gs()=%x", __read_gs(0x10));
        kprintf("\n__read_task(void)=%x", __read_gs_task());
        // kprintf("\nTSS=%x", __get_tss());
        kprintf("\ntss->IST3=%x", __tss_ist_read(__get_curr_tss(), TSS_IST3));
        // kprintf("\ntss->RSP0=%x", __tss_rsp_read(__get_tss(), TSS_RSP0));
        // kprintf("\ncurr_task->ist_rsp=%x", curr_task->ist_rsp);
        // kprintf("\ncurr_task->ist_rbp=%x", curr_task->ist_rbp);
        kprintf("\nRSP=%x", __read_reg_rsp());
        kprintf("\n&a=%x", &a);
        // on = false;
    }
}
