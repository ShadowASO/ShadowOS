;/*--------------------------------------------------------------------------
;*  File name:  entry.s
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 10-05-2020
;*--------------------------------------------------------------------------
;Este arquivo possui as rotinas para a mudança de contexto
;--------------------------------------------------------------------------*/
%include "sys.inc"

section .text
align 16
global long_mode_entry
long_mode_entry:
	
	; Long mode doesn't care about most segments.
	; GS and FS base addresses can be set with MSRs.
	; Tive muitos problemas com o gerenciador de interrupções porque
	; comentei as linhas abaixo, que zeram o segmentos
	mov ax, 0
	mov ss, ax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	; Align the stack to 16 bytes
	and rsp, ~0xF

	; Multiboot structure (physical address)
	push rbx
	sub rsp, 8


	; Initialise VGA textmode driver
	;extern vga_tmode_init
	;call vga_tmode_init

	;Carrega a tabela IDT(idt64) com os seus descritores(registros)
	;cli
	
	extern setup_idt
	call setup_idt

	; Set up BSP per-CPU data area
	extern percpu_init_bsp
	call percpu_init_bsp

		
	; Initialise scheduler (set dummy task)
	;extern sched_init
	;call sched_init
		

	; Load interrupt descriptor table
	
	;extern idt_init
	;call idt_init
	extern interrupt_ini
	call interrupt_ini

	;extern idt_load
	;call idt_load

	;------------------------------	
	; Carrega a TSS do BSP
	mov ax, GDT_TSS
	ltr ax
	;---------------------------------
	
	

	; Enable SSE, AVX, AVX-512
	;extern simd_init
	;call simd_init

	; Enable syscall/sysret instruction
	extern syscall_enable
	call syscall_enable

	; Call global constructors
	;extern _init
	;call _init
	;sti

	; Pass multiboot information to kmain
	add rsp, 8
	pop rdi
	extern kmain
	call kmain

	; Don't call global destructors - we should never get here
	sti
	;cli
.end:
	hlt
	jmp .end
	
	

