/*--------------------------------------------------------------------------
 *  File name:  runq.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 10-04-2022
 *--------------------------------------------------------------------------
 *  Rotina principal de entrada no kernel.
 *--------------------------------------------------------------------------*/
#include "list.h"
#include "percpu.h"
#include "task.h"
#include "rbtree.h"
#include "ktypes.h"
#include "sync/spin.h"
#include "smp.h"
#include "scheduler.h"
#include "lapic.h"
#include "runq.h"

/*
A Estrutura RUNQ possui um campo chamado "idle" que deve aponta para uma
task com um loop infinito, quando não existam quaisquer task na fila. Esse
campo não é modificado ao longo da vida do core.
ATENÇÃO: Esta rotina deve ser chamada uma única vez por cada CORE.
*/
void runq_init(struct task *idle)
{
    struct runq *rq = kmalloc(sizeof(struct runq));
    memset(rq, 0, sizeof(struct runq));

    for (int i = 0; i <= MAX_PRIO; i++)
    {
        init_list_head(&rq->arrays.queue[i]);
    }

    /* Atribuimos a nova fila de execução ao PERCPU do core que está
    executando o código. percpu->run_queue(struct runq *run_queue). */
    percpu_queue_set(rq);

    /* Grava o número da CPU no task. */
    set_task_cpu(idle, percpu_cpu_id());

    /* Faz o ponteiro array apontar para o array de filas em rq.*/
    set_task_array(idle, rq);

    /*    Gravo na estrutura da fila de execução a idle_task(com um loop infinito) recebida para
    ser utilizada quando não a fila estiver vazia e sem qualquer task útil a ser executada.  Sem
    isso há erro, por conta da lógica do task switch.  */
    rq->idle = idle;
    rq->nr_threads = 0;
    /*-------------------------------------------------------------------------------------------*/
}
/*
 * Adding/removing a task to/from a priority array:
 */
static void dequeue_task(struct task *p, prio_array_t *array)
{
    array->nr_active--;
    list_del(&p->run_list);
    // if (list_empty(array->queue + p->priority))
    //     __clear_bit(p->priority, array->bitmap);
}

static void enqueue_task(struct task *p, prio_array_t *array)
{
    // sched_info_queued(p);
    list_add_tail(&p->run_list, array->queue + p->priority);
    //__set_bit(p->prio, array->bitmap);
    array->nr_active++;
    p->array = array;
}
/*
 * Put task to the end of the run list without the overhead of dequeue
 * followed by enqueue.
 */
static void requeue_task(struct task *p, prio_array_t *array)
{
    list_move_tail(&p->run_list, array->queue + p->priority);
}
/* Procura a primeira fila no array que possui tasks. */
int runq_find_first_queue(struct runq *rq)
{
    struct list_head *head = NULL;

    for (size_t i = 0; i < MAX_PRIO; i++)
    {
        head = &rq->arrays.queue[i];
        if (!list_is_empty(head))
        {
            return i;
        }
    }
    return -1;
}
void runq_requeue(struct task *t, u8_t cpu)
{
    /* Essa atribuição é essencial, pois grava no task o CORE da requeue.
    Ela está aqui porque pode haver mudança no CORE durante a requeue. */
    set_task_cpu(t, cpu);

    /* Faz um lock. */
    struct runq *rq = task_runq_lock(t);

    requeue_task(t, t->array);

    /* Faz unlock. */
    task_runq_unlock(rq);
}

void runq_add(struct task *t, u8_t cpu)
{
    /* Essa atribuição é essencial, pois grava no task o CORE da runqueue. */
    set_task_cpu(t, cpu);

    /* Faz um lock. */
    struct runq *rq = task_runq_lock(t);

    set_task_array(t, rq);
    rq->nr_threads++;
    enqueue_task(t, t->array);

    /* Faz unlock. */
    task_runq_unlock(rq);
}

// Pops the next task from the rbtree
struct task *runq_next(u8_t cpu)
{
    struct runq *rq = get_runq_by_core(cpu);
    prio_array_t *array = &rq->arrays;
    task_t *next = NULL;
    int idx = runq_find_first_queue(rq);

    if (idx < 0)
    {
        return rq->idle;
    }
    struct list_head *head = array->queue + idx;

    if (list_is_empty(head))
    {
        next = rq->idle;
    }
    else
    {
        next = list_entry(head->next, task_t, run_list);
        // kprintf("\ncpu[ %d ]: PID[ %d ] = next=%p - IDX=%d", cpu, next->pid, next, idx);
    }

    return next;
}

void runq_remove(struct task *t)
{
    /* Faz um lock. */
    struct runq *rq = task_runq_lock(t);

    rq->nr_threads--;
    dequeue_task(t, t->array);

    /* Faz unlock. */
    task_runq_unlock(rq);
}
