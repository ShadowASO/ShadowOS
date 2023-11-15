%include "sys.inc"
%include "x86_64.inc"
%include "syscall.inc"

;----------------------------------------------
; Seletores
;----------------------------------------------
extern _GDT_TSS_SEL
extern _GDT_USER_CODE32_SEL

extern _GDT_KERNEL_CODE_SEL
extern _GDT_KERNEL_DATA_SEL
extern _GDT_USER_CODE_SEL
extern _GDT_USER_DATA_SEL

section .rodata
no_syscall_message:
	db 0x1B, 'Fatal error: syscall/sysret are not supported', 0xA, 0 ; Newline, NUL
	
section .text
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Enables the 'syscall' and 'sysret' instructions
; ** ATENÇÃO: Precisa ser ativado em cada CORE. **
global syscall_enable
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
syscall_enable:
	;-------------------------------------------------------------
	; Check that syscall and sysret are actually available
	mov eax, 0x80000001
	xor ecx, ecx
	cpuid
	test edx, (1 << 11)
	jz .no_syscall
	;------------------------------------------------------------

	; IA32_STAR MSR
	mov ecx, 0xC0000081
	rdmsr
	; Load code selector(CS) of return user into STAR[63:48] and 
	; Load code selector(CS) of target kernel code into STAR[47:32]
	mov edx, 0x00180008		
	wrmsr

	; IA32_LSTAR MSR
	mov ecx, 0xC0000082
	rdmsr
	; Load the start code (rip) to execute into IA32_LSTAR
	mov rdi, syscall_entry  ;Endereço da rotina a ser chamada.
	mov eax, edi
	shr rdi, 32
	mov edx, edi
	wrmsr

	; Enable both instructions
	mov ecx, 0xC0000080 ; IA32_EFER MSR
	rdmsr
	or eax, (1 << 0)
	wrmsr

	; Set FMASK MSR for rflags
	mov ecx, 0xC0000084 ; IA32_FMASK MSR
	rdmsr
	or eax, (1 << 9) | (1 << 10) ; Disable interrupts, clear direction flag
	;or eax, (1 << 9) ; Disable interrupts, clear direction flag
	wrmsr

	ret
;-----------------------------------------------------------
.no_syscall:
	mov rdi, no_syscall_message
	xor rax, rax ; Needed for varargs
	extern kprintf
	call kprintf
	;extern abort
	;jmp abort


;/*
; * System call entry. Upto 6 arguments in registers are supported.
; *
; * SYSCALL does not save anything on the stack and does not change the
; * stack pointer. Por isso, temos que fazer tudo isso nesta função.
; */
		
;/*
; * Register setup:	
; * rax  system call number
; * rdi  arg1 * 
; * rsi  arg2
; * rdx  arg3	
; * r8   arg4 	(--> moved to rcx for C)
; * r9   arg5
; *
; *------------------------
; * RCX == RIP (return address-next instruction)
; * r11 == RFLAGS(eflags for syscall/sysret, temporary for C)
; *------------------------------------------------------------
; * r12-r15,rbp,rbx saved by C code, not touched. 		
; * 
; * Interrupts are off on entry.
; * Only called from user space.
; *
; * XXX	if we had a free scratch register we could save the RSP into the stack frame
; *      and report it properly in ps. Unfortunately we haven't.
; *
; * When user can change the frames always force IRET. That is because
; * it deals with uncanonical addresses better. SYSRET has trouble
; * with them due to bugs in both AMD and Intel CPUs.
; */ 			 		
syscall_entry:		
	
	; Switch to kernel stack	
	mov [PERCPU_RSP_SCRATCH], rsp ; salvo rsp anterior(USER_STACK)
	mov rsp, [PERCPU_CURRENT]	
	mov rsp, [RSP + TASK_STACK_RBP]	; O RSP deve apontar para task->stack_rbp
				
	;-------------------------------------------------
	;       SAVE FRAME ON STACK - Faremos um iretq
	;-------------------------------------------------			
	push qword (GDT_USER_DATA | 0x3)	; user data descriptor		
	push qword [PERCPU_RSP_SCRATCH] ; Old RSP		
	push qword R11 ; RFLAGS			
	push qword (GDT_USER_CODE | 0x3)	; user code descriptor		
	push qword rcx ; RIP
	;---------------------------------------------------
	sti ; reativa as interrupções	
					
	; Verifica se o número da syscall está dentro do intervalo válido
	cmp rax, NUM_SYSCALLS
	jae .bad_syscall
	;---------------------------------------------------------------

	; FORK recebe um tratamento diferenciado as outras syscall. Não usamos
	; a tabela.
	cmp rax, SYSCALL_FORK
	je .syscall_fork
	;----------------------------------------------------------------	
		
	; Call the syscall function
	extern syscall_table
	mov rax, [syscall_table + rax * 8]
	call rax
	;----------------------------------------------------------------	
	jmp .done

.bad_syscall:
	mov rax, ENOSYS
.done:		
	xor rcx, rcx
	xor r11, r11	

	iretq	

;----------------------------------------------------------------------------
;struct task *syscall_fork(int (*fn)(void *), void *args, u64_t flags);
;----------------------------------------------------------------------------
.syscall_fork:	
    ;Estamos utilizando a estrutura pt_regs_t para tratar os registros de cada task/thread,
	;por isso, precisamos ajustar a stack, inserir mais 8 bytes, relativo ao elemento 
	;pt_regs_t->fn_ret. Vamos salvar nele o endereço da próxima instrução(RIP) a ser executada,
	;que syscall salvou em RCX
	push rcx ; RIP
	;---------------------------------------------------------------------
	PUSH_ALL	; Salvamos todos os registros como estão
	;-------------------------------------------------------------------------
	;Verificamos se foi passado um valor NULL para a função/endereço onde iniciará
	;a execução do novo task. Se for um valor NULL, atribuímos o valor da próxima 
	;instrução que se segue à função syscall_exec().
	cmp rdi, 0
	jnz .next
	mov [rsp+9*8], rcx ; RIP
		
	.next:
	mov rdi, rdx
	mov rsi, rsp  ;regs->stack	
	
	extern do_fork
	call do_fork
	; RAX contém o valor de retorno de "do_fork". Esse valor será destruído quando fizermos
	; um POP_ALL. Por isso, modifico o valor de RAX na própria stack. Quando fizer a sobre-
	; posição, o fará com o valor correto de RAX. 
	mov [rsp+14*8], rax 
	;-------------------------------------------------------------------------------------					
	
	POP_ALL	 ; Recuperamos todos os registros como estavam na abertura da rotina
	pop rcx  ; Ajusto a stack retirando o elemento pt_regs_t->fn_ret
	;---------------------------------------------------------------
	jmp .done ; Volto para o handler da syscall daqui

;-------------------------------------------------------------------------------------------
;void syscall_exec(u64_t sys_no, u64_t arg1, u64_t arg2, u64_t arg3, u64_t arg4, u64_t arg5);
;-------------------------------------------------------------------------------------------
;sys_no=RDI
;arg1=RSI
;arg2=RDX
;arg3=RCX  o Valor de RCX será destruído pelo compilador. Seu valor precisa ser transferido.
;arg4=R8
;arg5=R9
;Os valores precisam ser rearranjados, antes de executar "syscall"
;----------------------------------------------------------
global syscall_exec
syscall_exec:
	mov RAX, rdi ; sys_no
	mov rdi, rsi ; arg1
	mov rsi, rdx ; arg2
	mov rdx, rcx ; arg3	
;------------------------
	syscall	
	ret
