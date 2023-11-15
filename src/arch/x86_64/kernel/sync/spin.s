;/*--------------------------------------------------------------------------
;*  File name:  spin.s
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 05-03-2021
;*--------------------------------------------------------------------------
;Este arquivo fonte possui diversos rotinas escritas em assembly para sincro-
;nização de processos com o uso de semáforo.
;****************************************************************************
;void spin_lock(volatile spinlock_t *lock);
;void spin_unlock(volatile spinlock_t *lock);
;bool spin_try_lock(volatile spinlock_t *lock);
;--------------------------------------------------------------------------*/
%include "sys.inc"

[bits 64]
section .text
;********************************************************
;Esta rotina cria uma espera ocupada (busy wait) na própria
;rotina assenbly, somente retornando quando consegue obter
;a trava exclusiva.
;********************************************************
global __spin_lock
__spin_lock:		
	mov rax, 1
	xchg [rdi], rax
	test rax, rax
	jz .acquired
.retry:
	pause
	bt qword [rdi], 0
	jc .retry

	xchg [rdi], rax
	test rax, rax
	jnz .retry
.acquired:	
	ret

;***********************************************************
;Esta rotina faz apenas uma tentativa de obter a trava exclusiva,
;retornando um valor verdadeiro !=0 ou falso==0
;Fica a cargo do usuário, criar uma espera ocupada em C ou outra
;linguagem que preferir.
;****************************************************************
global __spin_trylock
__spin_trylock:	
	mov rax, 1
	xchg [rdi], rax
	test rax, rax
	jz .acquired	
	mov rax, 0	
	ret
.acquired:
	mov rax, 1	
	ret
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
global __spin_unlock
__spin_unlock:	
	mov qword [rdi], 0	
	ret
