/*--------------------------------------------------------------------------
*  File name:  timer.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 02-09-2023
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
#include "time.h"
#include "timer.h"

CREATE_LIST_HEAD(timer_head);

void run_timers(void)
{
    list_head_t *p = NULL;
    struct timer_list *timer = NULL;
    void (*fn)(u64_t);
    u64_t data;

    list_for_each(p, &timer_head)
    {
        timer = list_entry(p, struct timer_list, entry);

        if (timer->expires < get_jiffies())
        {
            fn = timer->function;
            data = timer->data;

            del_timer(timer);

            fn(data);

            if (list_is_empty(&timer_head))
                return;
        }
    }
}