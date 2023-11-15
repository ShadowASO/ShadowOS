/*--------------------------------------------------------------------------
*  File name:  fork.h
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 08-10-2022
*--------------------------------------------------------------------------
Este header reune estruturas para o Page Memory Allocator
--------------------------------------------------------------------------*/
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"
#include "../include/kernel.h"

task_t *fork(virt_addr_t fn, void *args, u64_t flags);
u32_t execve(virt_addr_t tsk, const char *argv[], const char *const envp[]);
u32_t getcpu(u32_t *cpup, u32_t *nodep);
void yield(void);
u32_t getpid(void);
u32_t kill(pid_t pid, u32_t sig);