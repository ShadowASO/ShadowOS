/*---------------------------------------------------------------------------------------------
 *  File name:  task.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 10-05-2021
 *---------------------------------------------------------------------------------------------
 * Todo o funcionamento do SMB é fundado na troca de contextos, com a divisão do tempo de uso
 * do CORE entre os diversos processos/threads. A troca de contextos no x86_64 é baseado no
 * legacy stack-switch mecanism. Há sempre a troca da stack quando houver uma mudança no nível
 * de privilégio (CPL(privilege level)).
 *
 * Uma thread criada no Mode Kernel nunca sofrerá a troca da stack por ocasião de uma interrup-
 * ção do timer. Ela salvará o seu contexto(registros) na própria stack.
 *
 * Quando ocorre uma interrupção com troca de contexto, o processador, automaticamente, troca
 * a stack para o endereço contido em TSS->RSP0. Por isso, durante a troca de contexto, preci-
 * samos grava em TSS->RSP0 a kernel mode stack do próximo task. Será nela que o sistema sal-
 * vará o contexto, quanedo for trocar por outro processo.
 *
 * As interrupções e exceções também não provocarão mudança na stack, quando o processo estiver
 * no Kernel Mode.
 *
 * O sistema somente troca automaticamente a stack quando ocorre uma interrupção no User Mode,
 * pois os dados precisam ser protegidos.
 *
 * Assim, a stack informada em TSS->RSP0 somente tem relevância quando ocorre uma
 * interrupção em modo usuário para o modo kernel.
 *-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -*/

#include "task.h"
#include "lapic.h"
#include "sync/mutex.h"
#include "hpet.h"
#include "shell.h"
#include "isr.h"
#include "percpu.h"
#include "runq.h"
#include "interrupt.h"
#include "../drivers/graphic/console.h"
#include "smp.h"
#include "scheduler.h"
#include "gdt.h"
#include "mm/vmalloc.h"

/* O contador global de available process ID. */
static atomic32_t next_pid = {PID_IDLE};

/* Ponteiro para a tabela com os endereços de todos Process Descriptors(task), ordenados
pelo PID. A tabela é criada dinamicamente. */
struct task **pidTable = NULL;

/* Vetor que reune o process descritor/kernel task de cada núcleo do sistema.
Reservamos uma união descriptor/stack para cada core no sistema*/
__attribute__((aligned(0x1000))) task_union_t init_by_cpu[MAX_CORES] = {0};

/* Devolve o próximo ID disponível, de forma atomic. */
static pid_t get_next_pid(void)
{
    return atomic_inc_read32(&next_pid);
}
/* Grava o process descriptor address na tabela 'pidTable'. */
static void set_table_pid(pid_t pid, struct task *ptask)
{
    pidTable[pid] = ptask;
}
/* Devolve o endereço do process descriptor (task) correspondente ao PID indicado. */
struct task *find_task_by_pid(pid_t pid)
{
    return pidTable[pid];
}
/* Retorna o process descriptor (task) do init_task 'idle' de cada CORE. */
static struct task *get_task_idle(u8_t cpu)
{
    struct runq *rq = percpu_by_core(cpu)->run_queue;
    return rq->idle;
}

static struct runq *get_task_runq(struct task *t)
{
    u8_t cpu = get_task_cpu(t);
    return get_runq_by_core(cpu);
}
/* Task que não faz nada, além de ser o primeiro da fila de cada core. Será executado
quando a fila de tasks de cada core estiver vazia. */
static void cpu_idle(void)
{
    while (true)
    {
        __PAUSE__();
        __HLT__();
    }
}
/* Configura e devolve um união descritor/task para cada core, a partir do vetor
init_by_cpu. */
struct task *get_init_task(u8_t cpu_id)
{
    /*
    Seleciona o elemento utilizando o número do core.

    Esta estrutura terá apenas o espaço de memória necessários para guardar
    as informações do task descriptor e o kernel mode stack.

    Existe uma stack exclusiva para cada task para uso quando ele entra no
    kernel mode, durante uma troca de contexto. Ela guarda o contexto   do
    processo/thread.

    Ela será sempre referenciada por 'rsp0' e gravada no TSS->RSP0.

    A stack de execução do aplicativo/programa, no user mode,  deve ser con-
    trolada em outras estruturas(mm_struct).
    */
    task_t *task_new = &init_by_cpu[cpu_id].task;

    /* Faço a limpeza do espaço de memória reservada. */
    clear_vm_range(task_new, PAGE_SIZE * 2);

    /* Atributos básicos do task inicial, que executará uma rotina com
    um loop infinito. */
    task_new->pid = PID_IDLE;       // INIT_TASK_PID;
    task_new->parent_id = PID_IDLE; // INIT_TASK_PID;
    task_new->state = eSTATE_RUNNING;
    task_new->flags = TASK_IDLER | THREAD_KERNEL | MODE_KERNEL;

    /* Atribui a prioridade do task. */
    set_task_priority(task_new, 0);

    /* Atribui o endereço do task à tabela pidTable. */
    set_table_pid(task_new->pid, task_new);

    reset_task_time(task_new, TASK_SLICES_SYS);

    /* stack_rbp vai apontar para o início da stack. */
    task_new->rsp0 = (mm_addr_t)incptr(task_new, (PAGE_SIZE * 2) - 16);
    task_new->stack_rbp = (virt_addr_t)task_new->rsp0;

    /* Crio o stack frame inicial */
    uint64_t *stack_rsp = task_new->stack_rbp;

    // WARN_ON("CORE=[%d]-->PID=[ %d ]: task_new=%p", cpu_id, task_new->pid, task_new);
    /*--------------------------------------------------*/
    *--stack_rsp = _GDT_KERNEL_DATA_SEL;        // ss
    *--stack_rsp = (u64_t)task_new->stack_rbp;  // rsp
    *--stack_rsp = (__read_rflags64() | 0x200); // rflags
    *--stack_rsp = _GDT_KERNEL_CODE_SEL;        // cs
    *--stack_rsp = (u64_t)cpu_idle;             // rip
    /*--------------------------------------------------*/

    /* Onde o switch_to irá returnar, na primeira execução. */
    *--stack_rsp = (u64_t)__ret_first_switch; /* Chamada no Primeiro switch. */

    *--stack_rsp = 0x0; // rax
    *--stack_rsp = 0x0; // rbx
    *--stack_rsp = 0x0; // rcx
    *--stack_rsp = 0x0; // rdx
    *--stack_rsp = 0x0; // rsi
    *--stack_rsp = 0x0; // rdi
    *--stack_rsp = 0x0; // rbp
    *--stack_rsp = 0x0; // r8
    *--stack_rsp = 0x0; // r9
    *--stack_rsp = 0x0; // r10
    *--stack_rsp = 0x0; // r11
    *--stack_rsp = 0x0; // r12
    *--stack_rsp = 0x0; // r13
    *--stack_rsp = 0x0; // r14
    *--stack_rsp = 0x0; // r15

    /* Fixo o ponteiro do registro RSP para o topo da stack. */
    task_new->stack_rsp = stack_rsp;

    return task_new;
}

static struct task *copy_task(task_t *parent, virt_addr_t entry, uint64_t flags, pt_regs_t *regs, pid_t pid)
{
    task_t *task_new = vmalloc(PAGE_SIZE * 2); /* Reservo duas página. */
    clear_vm_range(task_new, PAGE_SIZE * 2);
    task_new->rsp0 = (mm_addr_t)incptr(task_new, (PAGE_SIZE * 2) - 16);

    task_new->pid = pid;
    task_new->parent_id = parent->pid;
    task_new->state = eSTATE_NEW;
    task_new->flags = flags;

    /* Atribui a prioridade do task. */
    set_task_priority(task_new, 0);

    reset_task_time(task_new, TASK_SLICES_SYS);

    return task_new;
}

struct task *task_fork(task_t *parent, virt_addr_t entry, uint64_t flags, pt_regs_t *regs)
{
    pid_t pid = get_next_pid();
    task_t *task_new = copy_task(parent, entry, flags, regs, pid);
    virt_addr_t user_stk = NULL;

    /* Atribuo o endereço do task à tabela pidTable. */
    set_table_pid(task_new->pid, task_new);

    // WARN_ON("PID=[ %d ]: task_new=%p", task_new->pid, task_new);

    /* Kernel Mode stack*/
    task_new->stack_rbp = (virt_addr_t)task_new->rsp0;

    /* Se o fork for para criação de um thread/processo no mode kernel, não será preciso
    criar uma stack. Nos casos de thread/processo no User mode, precisamos de uma stack
    de execução no level 3(user mode), já que a kernel mode task é exclusivamente para o
    momento em que o task entra em modo kernel gerado por interrupções ou syscall. */

    if ((flags & MODE_USER))
    {
        /* Crio uma stack de trabalho para o task no mode user.*/
        virt_addr_t stk = vmalloc(PAGE_SIZE * 2); /* Reservo duas página. */
        clear_vm_range(stk, PAGE_SIZE * 2);
        user_stk = (incptr(stk, (PAGE_SIZE * 2) - 16));

        // WARN_ON("USER MODE: PID=[ %d ]: kernel stack=%p - User stack=%p", task_new->pid, task_new->rsp0, user_stk);
        //  dump_task_regs(regs); /* Dump do frame recebido. */
    }
    else
    {
        // task_new->stack_rbp = (virt_addr_t)task_new->rsp0;
        // WARN_ON("KERNEL MODE: kernel stack=%p - User stack=%p", task_new->rsp0, 0);
    }

    /*------------------------------------------------------------------*/
    /* Crio o stack frame inicial, que deve ser gravado na kernel mode
    stack(rsp0). */
    pt_regs_t *stack_rsp = decptr(task_new->stack_rbp, sizeof(pt_regs_t));

    /* No caso de uma thread do kernel, somente o próprio kernel poderá criá-la e ela
    possuirá os mesmos atributos do processo que a criou. */
    if (flags & THREAD_KERNEL)
    {
        stack_rsp->ss = read_SS();                   // ss;
        stack_rsp->rsp = (u64_t)task_new->stack_rbp; // rsp
        stack_rsp->rflags = (__read_rflags64());     // rflags;
        stack_rsp->cs = read_CS();                   // cs;
        stack_rsp->rip = (u64_t)entry;               // rip;
    }
    else if (flags & MODE_KERNEL)
    {
        stack_rsp->ss = _GDT_KERNEL_DATA_SEL;            // ss;
        stack_rsp->rsp = (u64_t)task_new->stack_rbp;     // rsp
        stack_rsp->rflags = (__read_rflags64() | 0x200); // rflags;
        stack_rsp->cs = _GDT_KERNEL_CODE_SEL;            // cs;
        stack_rsp->rip = (u64_t)entry;                   // rip;
    }
    else if (flags & MODE_USER)
    {
        stack_rsp->ss = (_GDT_USER_DATA_SEL | 0x3);      // ss
        stack_rsp->rsp = (u64_t)user_stk;                // rsp
        stack_rsp->rflags = (__read_rflags64() | 0x200); // rflags        //
        stack_rsp->cs = (_GDT_USER_CODE_SEL | 0x3);      // cs
        stack_rsp->rip = (u64_t)entry;                   // rip;
    }

    /* Onde o switch_to irá returnar, na primeira execução. */
    stack_rsp->fn_ret = (u64_t)__ret_first_switch; /* Chamada no Primeiro switch. Peguei emprestado. */

    stack_rsp->rax = 0;
    stack_rsp->rbx = 0;
    stack_rsp->rcx = 0;
    stack_rsp->rdx = 0;
    stack_rsp->rsi = 0;
    stack_rsp->rdi = 0;
    stack_rsp->rbp = 0;
    stack_rsp->r8 = 0;
    stack_rsp->r9 = 0;
    stack_rsp->r10 = 0;
    stack_rsp->r11 = 0;
    stack_rsp->r12 = 0;
    stack_rsp->r13 = 0;
    stack_rsp->r14 = 0;
    stack_rsp->r15 = 0;

    if (regs)
    {
        stack_rsp->rdi = regs->rsi;
        stack_rsp->rsi = regs->rsi;
        stack_rsp->rdx = regs->rdx;
        stack_rsp->rcx = regs->rcx;
        stack_rsp->r8 = regs->r8;
        stack_rsp->r9 = regs->r9;
    }

    /* Fixo o ponteiro do registro RSP para o topo da stack. */
    task_new->stack_rsp = (virt_addr_t)stack_rsp;

    // dump_task_regs(stack_rsp); /* Dump do frame criado. */

    return task_new;
}

/* regs aponta para os registros salvos na stack. */
task_t *do_fork(uint64_t flags, pt_regs_t *regs)
{
    struct task *t = task_fork(percpu_current(), (virt_addr_t)regs->rdi, flags, regs);

    return t;
}
/* Cria uma thread e aceita a passagem de um parâmetro para o thread. */
task_t *pthread_create(int (*func)(void *), void *argv, uint64_t flags)
{
    task_t *t = kthread_create(func, argv, flags);

    return t;
}

/**
 * @brief  Reseta os contadores de tempo do task e atribui-lhe uma
 * fatia(slice) de tempo
 * @note   Deve ser usada principalmente na criação de uno novo task
 * @param  *task:
 * @param  slice:
 * @retval None
 */
void reset_task_time(task_t *task, u64_t slices)
{
    task->timer.slice_nr = slices;
    task->timer.used_time_ms = 0;
    //**** tempo de execução em um slice
    task->timer.init_exec_timer_ms = 0;
    task->timer.used_exec_time_ms = 0;
}
/**
 * @brief  Atribui ao task a fatia(slice) de tempo que ele terá para
 * ser executado a cada ciclo.
 * @note
 * @param  *task:
 * @param  slice:
 * @retval None
 */
static inline void set_task_slice_time(task_t *task, u64_t slices)
{
    task->timer.slice_nr = slices;
    task->timer.init_exec_timer_ms = 0;
    task->timer.used_exec_time_ms = 0;
}
/**
 * @brief  Atualiza o tempo total de utilização do task e o total
 * de execução a cada ciclo
 * @note
 * @param  *task:
 * @retval None
 */
static inline void update_task_slice_used(task_t *task)
{
    u64_t time_ms = hpet_counter_on_milli() - task->timer.init_exec_timer_ms;
    task->timer.used_time_ms = task->timer.used_time_ms + time_ms;
    task->timer.used_exec_time_ms = task->timer.used_exec_time_ms + time_ms;
}
/**
 * @brief  Verifica se a fatia de tempo foi utilizada integralmente no ciclo
 * de execução.
 * @note
 * @param  *task:
 * @retval
 */
static inline bool is_task_slice_finish(task_t *task)
{
    return (task->timer.slice_nr);
}
/**
 * @brief
 * @note
 * @param  *task:
 * @retval None
 */
static inline void init_task_exec_slice(task_t *task)
{
    task->timer.init_exec_timer_ms = hpet_counter_on_milli();
    task->timer.used_exec_time_ms = 0;
}

void dump_task_regs(pt_regs_t *task_context)
{
    // u64_t *tss = __get_curr_tss();
    u64_t *stack = 0;

    // kprintf("\nTSS_IST3=%x", __tss_ist_read(tss, TSS_IST3));
    kprintf("\ntask_context->ss=%x", task_context->ss);
    kprintf("\ntask_context->rsp=%p", task_context->rsp);
    kprintf("\ntask_context->rflags=%x", task_context->rflags);
    kprintf("\ntask_context->cs=%x", task_context->cs);
    kprintf("\ntask_context->rip=%p", task_context->rip);

    kprintf("\ntask_context->fn_ret=%p", task_context->fn_ret);

    kprintf("\ntask_context->rax=%x", task_context->rax);
    kprintf("\ntask_context->rbx=%x", task_context->rbx);
    kprintf("\ntask_context->rcx=%x", task_context->rcx);
    kprintf("\ntask_context->rdx=%x", task_context->rdx);
    kprintf("\ntask_context->rsi=%x", task_context->rsi);
    kprintf("\ntask_context->rdi=%x", task_context->rdi);
    kprintf("\ntask_context->rbp=%x", task_context->rbp);
    kprintf("\ntask_context->r8=%x", task_context->r8);
    kprintf("\ntask_context->r9=%x", task_context->r9);
    kprintf("\ntask_context->r10=%x", task_context->r10);
    kprintf("\ntask_context->r11=%x", task_context->r11);
    kprintf("\ntask_context->r12=%x", task_context->r12);
    kprintf("\ntask_context->r13=%x", task_context->r13);
    kprintf("\ntask_context->r14=%x", task_context->r14);
    kprintf("\ntask_context->r15=%x", task_context->r15);
}