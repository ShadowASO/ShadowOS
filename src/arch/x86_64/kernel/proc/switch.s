;/*--------------------------------------------------------------------------
;*  File name:  switch.s
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 10-05-2021
;*--------------------------------------------------------------------------
;Este arquivo possui as rotinas para a mudança de contexto
;--------------------------------------------------------------------------*/
%include "sys.inc"
%include "x86_64.inc"

bits 64
section .text

;align 16
extern tss64


global flush_gdt_tss
flush_gdt_tss:
	; rdi: gdt
	; rsi: GDT_SIZE
	; rdx: tss

	sub rsp, 10
	mov word [rsp], si
	mov qword [rsp + 2], rdi

	lgdt [rsp]
	mov ax, GDT_TSS
	ltr ax

	add rsp, 10
	ret
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;u64_t __testa_segment_mem(void)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
global __testa_segment_mem
__testa_segment_mem:	
	;mov qword[PERCPU_PREEMPT_COUNT], 11
	mov rdi, 11
	mov PERCPU_PREEMPT_COUNT, rdi
	;lea rax, [PERCPU_PREEMPT_COUNT]
	mov rax, PERCPU_PREEMPT_COUNT		
	ret

;*****************************************************************************************
;Aplica o endereço da nova pilha no TSS
;void __tss_ist_write(u64_t *tss , u64_t ist_offset, u64_t stack_adddress);
;*****************************************************************************************
; RDI = tss
; RSI = ist_offset
; RDX = stack_address
;****************************************************************************************
;extern tss64
global __tss_ist_write
__tss_ist_write:	
	cli	
	mov [rdi + rsi], rdx		
	sti
	ret

;*****************************************************************************************
;Aplica o endereço da nova pilha no TSS
;u64_t __tss_ist_read(u64_t *tss , u64_t ist_offset);
;*****************************************************************************************
; RDI = tss
; RSI = ist_stack
;****************************************************************************************
;extern tss64
global __tss_ist_read
__tss_ist_read:		
	mov rax, [rdi + rsi]
	ret	

;*****************************************************************************************
;Troca o RSPx do TSS
;void __tss_rsp_write(u64_t *tss , u64_t rsp_offset, u64_t rsp_adddress);
;*****************************************************************************************
;RDI = tss
;RSI = ist_offset
;RDX = rsp_address
;****************************************************************************************
global __tss_rsp_write
__tss_rsp_write:	
	cli
	mov [rdi + rsi], rdx		
	sti
	ret

;*****************************************************************************************
;Ler o RSPx do TSS
;u64_t __tss_rsp_read(u64_t *tss , u64_t rsp_offset) //;(int tss_rspx)
;*****************************************************************************************
;RDI = tss table
;RSI = rsp_offset
;****************************************************************************************
global __tss_rsp_read
__tss_rsp_read:	
	mov rax, [rdi + rsi]
	ret	

;*****************************************************************************************
;Devolve o endereço do TSS a partir do registro GS
;u64_t * __get_tss(void);
;*****************************************************************************************
;****************************************************************************************
global __get_curr_tss
__get_curr_tss:
	mov rax, PERCPU_TSS
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;void preempt_disable(void)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
global __preempt_disable
__preempt_disable:
	lock inc dword [gs:0x18]
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;void preempt_enable(void)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
global __preempt_enable
__preempt_enable:
	lock dec dword [gs:0x18]
	ret


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;*****************************************************************************************
;Provoca a primeira execução de um task recém criado. Seu return executa a task frame provi-
;sória utilizada na criação do task. 
;*****************************************************************************************
;void ret_from_execve(u64_t rsp_stack);
;****************************************************************************************
;RDI: o ponteiro para um task
;****************************************************************************************
global ret_from_execve, set_task_percpu
ret_from_execve:
	;mov rsp, [rdi +TASK_IST_RSP] 
	mov rsp, [rdi +TASK_STACK_RSP]
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rbx
	pop rax ;endereço da __ret_from_ufork(void)	
	iretq

;*****************************************************************************************
;Faz a primeira execução de um task recém criado. Seu return executa a task frame, fazendo
;o task entrar em execução, pois antes ele permanecia na lista de eSTATE_READY
;*****************************************************************************************
;void __ret_first_switch(void);
;****************************************************************************************
global __ret_first_switch
__ret_first_switch:
	mov rax, 0
	iretq

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Faz a troca de contexto a partir da estrutura de contexto que é passada.
;Na primeira interação de cada percpu, os push são realizados na pilha do 
;kernel em cada CPU. Nas trocas seguintes, geradas por interrupção, os dados são salvos no
;stack do task em execução. Não há perda de dados.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;void __switch_to(void * next_task);
;-----------------------------------------------------------------------------------------
;RDI = curr_task;
;(*) Todas as referência à stack nesta rotina se referem ao IST2 de cada task.
;-----------------------------------------------------------------------------------------
global __switch_to
__switch_to:
	cli	
	
	PUSH_ALL

	;----------------------------------------------------------------		
	;Copio o endereço do task atual(PERCPU_CURRENT gs:0x0) no registro RAX. 
	mov rax, PERCPU_CURRENT 
	;Agora salvo a posição atual do RSP no campo task->stack_rsp;	 		
	mov [rax + TASK_STACK_RSP], rsp	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;                  PRÓXIMO TASK
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;Gravo no PERCPU o novo task(next) como "current task" 
	;(PERCPU_CURRENT gs:0x0). o next_task foi passado em 'RDI'	
	;****************************************************************
	mov [PERCPU_CURRENT], rdi  ;"next task"
	mov rsp, [rdi + TASK_STACK_RSP]	;RSP deve apontar para task->stack_rsp
		
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;Faço as modificações no TSS que usará o mecanismo IST.
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;	
	
	;mov rax, [PERCPU_TSS] 			; pego endereço do TSS
	;mov rsi, [rdi + TASK_IST_RBP]	; pego o campo task->ist_rbp	
	;mov [rax + TSS_IST2], rsi		; copio o task->ist_rbp p/o TSS->IST2
	;mov [rax + TSS_RSP0], rsi		; copio o task->ist_rbp p/o TSS->RSP0(*)

	;(*) O mesmo stack será utilizada pelo mecanismo IST2 e pelo 
	;legacy RSP0, para as exceções que modifiquem o level. Não pode
	;acontecer uma exceção durante uma troca de contexto. Resultado
	;imprevisível.

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;Modificado o RSP da stack, inverto o procedimento acima e faço
	;um pop dos registros gerais salvos anteriormente na stack
	;------------------------------------------	
	; Reativo a preempção, desativada na entrada do scheduler()
	__PREEMPT_ENABLE
	
	POP_ALL

	sti
	ret


;*****************************************************************************************
;Devolve o task gravado no GS
;void __read_gs_task(void);
;task_t * __read_gs_task(void);
;*****************************************************************************************
;RDI = curr_task;;
;****************************************************************************************
global __read_gs_task
__read_gs_task:  
    mov rax, [PERCPU_CURRENT]
    ret

;*****************************************************************************************
;Faz a inclusão do task no registro GS do PERCPU
;void __write_gs_task(u64_t task_init);
;*****************************************************************************************
;RDI: ((u64_t) *task)
;*****************************************************************************************
global __write_gs_task
__write_gs_task:
	mov [PERCPU_CURRENT], rdi  ;pego o endereço da estutura percpu (PERCPU_CURRENT gs:0x0)	
	ret
;---------------------------------------------------------------------------
;void __dump_regs(u64_t p[])
;Recebe o endereço de uma matriz com 17 posição que será preenchida pela rotina
;com os valores nos registros da CPU
;---------------------------------------------------------------------------
;RDI: ponteiro matriz de u64_t
;----------------------------------------------------------------------------
global __dump_regs
global __dump_rax, __dump_rbx, __dump_rcx, __dump_rdx, __dump_rsi
global __dump_rdi, __dump_rbp, __dump_rsp, __dump_r8, __dump_r9,__dump_r10,__dump_r11
global __dump_r12,__dump_r13,__dump_r14, __dump_r15, __dump_rip

__dump_regs:	
	mov [rax + 8*0], rax
	mov [rax + 8*1], rbx
	mov [rax + 8*2], rcx
	mov [rax + 8*3], rdx
	mov [rax + 8*4], rsi
	mov [rax + 8*5], rdi
	mov [rax + 8*6], rbp
	mov [rax + 8*7], rsp
	mov [rax + 8*8], r8
	mov [rax + 8*9], r9
	mov [rax + 8*10], r10
	mov [rax + 8*11], r11
	mov [rax + 8*12], r12
	mov [rax + 8*13], r13
	mov [rax + 8*14], r14
	mov [rax + 8*15], r15
	mov rax, [rsp] ;RIP	
	mov [rdi + 8*16], rax
	ret

;Dump de Registros Gerais
__dump_rax:			
	ret
__dump_rbx:		
	mov rax, rbx	
	ret
__dump_rcx:		
	mov rax, rcx	
	ret
__dump_rdx:		
	mov rax, rdx	
	ret
__dump_rsi:		
	mov rax, rsi	
	ret
__dump_rdi:		
	mov rax, rdi	
	ret
__dump_rbp:		
	mov rax, rbp	
	ret
__dump_rsp:		
	mov rax, rsp	
	ret
__dump_r8:		
	mov rax, r8	
	ret
__dump_r9:		
	mov rax, r9	
	ret
__dump_r10:		
	mov rax, r10	
	ret
__dump_r11:		
	mov rax, r11	
	ret
__dump_r12:		
	mov rax, r12	
	ret
__dump_r13:		
	mov rax, r13	
	ret
__dump_r14:		
	mov rax, r14	
	ret
__dump_r15:		
	mov rax, r15	
	ret
__dump_rip:		
	mov rax, [rsp]	
	ret
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;global idle_task
;idle_task:
;.loop:	
;	sti
;	pause
;	hlt
;	jmp .loop

;*****************************************************************************************
;Cria uma thread do kernel
;struct task *ktread_create(int (*fn)(void *), void *args, u64_t flags);
;*****************************************************************************************
;RDI = fn;
;RSI = args
;RDX = flags
;****************************************************************************************
extern do_fork
global kthread_create
kthread_create:  
	;Estamos utilizando a estrutura pt_regs_t para tratar os registros de cada task/thread,
	;por isso, precisamos inserir mais 8 bytes, relativo ao elemento pt_regs_t->fn_ret
	push rdi
	;------------------------------------------------------------------------------------
    PUSH_ALL ; Salvamos todos os registros como estão
	
	mov rdi, rdx  ;flags
	mov rsi, rsp  ;regs->stack
	;args já se encontra salvo na stack	
	
	call do_fork
	; RAX contém o valor de retorno de "do_fork". Esse valor será destruído quando fizermos
	; um POP_ALL. Por isso, modifico o valor de RAX na própria stack. Quando fizer a sobre-
	; posição, o fará com o valor correto de RAX. 
	mov [rsp+14*8], rax 
	;-------------------------------------------------------------------------------------						
	
	POP_ALL	; Recuperamos todos os registros como estavam na abertura da rotina
	;----------------------------------------------------------------
	pop rdi ; Ajusto a stack retirando o elemento pt_regs_t->fn_ret
	;---------------------------------------------------------------
    ret


