/*--------------------------------------------------------------------------
*  File name:  fork.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 08-10-2022
*--------------------------------------------------------------------------
Este header reune estruturas para o Page Memory Allocator
--------------------------------------------------------------------------*/

#include <stdarg.h>
#include <stdbool.h>
#include "../user/printf.h"
#include "kernel.h"
#include "syscall/syscalls.h"
#include "task.h"
#include "../user/fork.h"

task_t *fork(virt_addr_t fn, void *args, u64_t flags)
{
    return (task_t *)syscall_exec(__NR_fork, (mm_addr_t)fn, (mm_addr_t)args, flags, 0, 0);
}
u32_t execve(virt_addr_t tsk, const char *argv[], const char *const envp[])
{
    return syscall_exec(__NR_execve, (mm_addr_t)tsk, (mm_addr_t)argv, (mm_addr_t)envp, 0, 0);
}
u32_t getcpu(u32_t *cpup, u32_t *nodep)
{
    return syscall_exec(__NR_getcpu, (mm_addr_t)cpup, (mm_addr_t)nodep, 0, 0, 0);
}
void yield(void)
{
    syscall_exec(__NR_sched_yield, 0, 0, 0, 0, 0);
}
u32_t getpid(void)
{
    return syscall_exec(__NR_getpid, 0, 0, 0, 0, 0);
}

u32_t kill(pid_t pid, u32_t sig)
{
    return syscall_exec(__NR_kill, pid, sig, 0, 0, 0);
}