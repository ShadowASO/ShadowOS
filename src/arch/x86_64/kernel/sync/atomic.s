;/*--------------------------------------------------------------------------
;*  File name:  atomic.s
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 18-06-2022
;*--------------------------------------------------------------------------
;Este arquivo fonte possui diversos rotinas escritas em assembly para opera-
;ções atomicas, mas no GCC existem Build-in functions que realizam as mesmas
;operações.
;****************************************************************************
;void __atomic64_inc(u64_t *value);
;void __atomic64_dec(u64_t *value);
;--------------------------------------------------------------------------*/
%include "sys.inc"

[bits 64]
section .text
;********************************************************
;Incrementa uma variável u64_t
;********************************************************
global __atomic64_inc
__atomic64_inc:	
	mov rax, rdi
    lock inc qword [rax]		
	ret

;********************************************************
;Incrementa uma variável u64_t
;********************************************************
global __atomic64_dec
__atomic64_dec:	
	mov rax, rdi
    lock dec qword[rax]	
	ret

;********************************************************
;Incrementa uma variável uint32_t
;********************************************************
global __atomic32_inc
__atomic32_inc:	
	mov rax, rdi
    lock inc dword[rax]	
	ret

;********************************************************
;Incrementa uma variável uint32_t
;********************************************************
global __atomic32_dec
__atomic32_dec:	
	mov rax, rdi
    lock dec dword[rax]	
	ret

