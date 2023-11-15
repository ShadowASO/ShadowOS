/*--------------------------------------------------------------------------
*  File name:  syscall.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 07-10-2023
*--------------------------------------------------------------------------
Este arquivo reune o código de todas as rotinas de serviço chamadas pela
syscall. Sempre que implementarmos uma nova rotina, ela deve ser atribuída
à tabela na rotina setup_syscall().
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"
#include "stdio.h"
#include "x86.h"
#include "task.h"
#include "percpu.h"
#include "lapic.h"
#include "syscalls.h"

#include <stdarg.h>
#include <stdbool.h>
#include "screen.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "../drivers/graphic/console.h"
#include "sync/spin.h"
#include "debug.h"
#include "percpu.h"
#include "task.h"
#include "scheduler.h"
#include "smp.h"

struct getcpu_cache;

syscret_t
sys_read(unsigned int fd, char *buf, size_t count)
{
    return kgetch();
}

syscret_t sys_write(unsigned int fd, const char *buf, size_t count)
{
    void *func = NULL;
    if (is_graphic_mode())
    {
        for (size_t i = 0; i < count; i++)
        {
            console_putchar(console_obj, buf[i]);
        }
    }
    else
    {
        for (size_t i = 0; i < count; i++)
        {
            putchar(buf[i]);
        }
    }

    return 0;
}
syscret_t sys_execve(task_t *task)
{
    return sched_execve(task);
}
syscret_t sys_yield(void)
{
    sched_yield();
    return 0;
}

syscret_t sys_getcpu(u32_t *cpup, u32_t *nodep)
{
    *cpup = smp_cpu_id();
    return *cpup;
}
syscret_t sys_getpid(void)
{
    return percpu_current()->pid;
}
syscret_t sys_kill(pid_t pid, u32_t sig)
{
    task_t *t = NULL;
    t = find_task_by_pid(pid);

    /* Desativa a preempção para esta CPU. */
    preempt_disable();
    t->state = eSTATE_ZOMBIE;
    /* Ativo a preempção para esta CPU. */
    preempt_enable();
    return sig;
}
syscret_t sys_exit(u32_t error_code)
{
    return error_code;
}
