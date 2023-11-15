/*--------------------------------------------------------------------------
 *  File name:  task.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 10-05-2021
 *--------------------------------------------------------------------------
 *  Rotina principal de entrada no kernel.
 *--------------------------------------------------------------------------*/
#include "task.h"
#include "sync/semaphore.h"
#include "list.h"
// #include "mutex.h"
#include "sync/spin.h"
#include "scheduler.h"

// Variável a ser utilizada como mutex
CREATE_SPINLOCK(spinlock_semaphore);

void semaphore_init(semaphore_t *s, TListNode_t *head_blocked)
{
    s->count = SEMAPHORE_STATE_INIT;
    // INIT_LIST_HEAD(head_blocked);
    init_list_head(head_blocked);
};

static inline task_t *get_first_node(semaphore_t *s)
{
    TListNode_t *head_node = &s->task_head_blocked;
    TListNode_t *tmp_node = head_node->next;
    task_t *tmp_task = NULL;

    if (tmp_node != head_node)
    {
        // tmp_task = list_entry(tmp_node, task_t, node);
    }
    return tmp_task;
}
static inline void append_block_task(semaphore_t *s, task_t *task)
{
    task->state = eSTATE_WAITING;
    // list_del(&task->node);
    // list_add_tail(&task->node, &s->task_head_blocked);
}

void semWait(semaphore_t *s, task_t *task)
{
    spinlock_lock(&spinlock_semaphore);
    s->count--;
    if (s->count < 0)
    {
        // insere na lista aguardando
        append_block_task(s, task);
    }

    spinlock_unlock(&spinlock_semaphore);
}

void semSignal(semaphore_t *s)
{
    spinlock_lock(&spinlock_semaphore);

    task_t *first_task = NULL;
    s->count++;

    if (s->count < SEMAPHORE_STATE_INIT)
    {
        first_task = get_first_node(s);
        if (first_task)
        {
            unblock_task(first_task);
        }
    }

    spinlock_unlock(&spinlock_semaphore);
}
