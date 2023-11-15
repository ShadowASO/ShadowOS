/*--------------------------------------------------------------------------
*  File name:  syscall_gen.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 07-10-2023
*--------------------------------------------------------------------------
Este arquivo fonte possui diversas funções relacionadas ao gerenciamento do
tempo.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"
#include "libc/stdio.h"
#include "x86.h"
#include "task.h"
#include "percpu.h"
#include "lapic.h"
#include "syscall.h"
#include "debug.h"
#include "syscalls.h"

long sys_null_syscall(void);

/* Tabela de System Service.*/
typedef void (*syscall_ptr_t)(void);
syscall_ptr_t syscall_table[__MAX_SYSCALL + 1] = {
	[0 ... __MAX_SYSCALL] = (syscall_ptr_t)sys_null_syscall};

/* Rotina utilitária para inserção de um novo syscall handler
na tabela syscall_table. */
static void set_syscall_handler(u32_t sys_nr, virt_addr_t sys_hd)
{
	syscall_table[sys_nr] = (syscall_ptr_t)sys_hd;
}
/*
Rotina usada na criação da tabela de syscall. Ela apresenta uma
mensagem de entra em loop. Deve ser substituída para cada handler
criado, por meio do setup_syscall().
*/
long sys_null_syscall(void)
{
	DBG_STOP("\nERROR: SYSCALL a implementar.");
	return -1;
}

/* Inserção dos Service handler na syscall table. Quando houver erro de inexe-
cução de uma syscall, verificar se o service handler foi inserido
na syscall table.*/
void setup_syscall(void)
{
	set_syscall_handler(__NR_read, sys_read);
	set_syscall_handler(__NR_write, sys_write);
	set_syscall_handler(__NR_execve, sys_execve);
	set_syscall_handler(__NR_sched_yield, sys_yield);
	set_syscall_handler(__NR_getcpu, sys_getcpu);
	set_syscall_handler(__NR_getpid, sys_getpid); //__NR_kill
	set_syscall_handler(__NR_kill, sys_kill);
}
