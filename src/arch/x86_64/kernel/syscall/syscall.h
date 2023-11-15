/*--------------------------------------------------------------------------
*  File name:  syscall.h
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 06-10-2023
*--------------------------------------------------------------------------
Este header reune os protótipos necessários para a criação da tabela de
syscall, utilizando uma rotina de serviço que não faz nada. Ou seja, o
sistema precisa inserir cada syscall que tenha sido criada.
--------------------------------------------------------------------------*/
#pragma once

#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"
#include "kcpuid.h"
#include "task.h"
#include "unistd_64.h"

extern void syscall_enable(void);
void setup_syscall(void);
int64_t syscall_exec(u64_t sys_no, u64_t arg1, u64_t arg2, u64_t arg3, u64_t arg4, u64_t arg5);
