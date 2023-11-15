;/*--------------------------------------------------------------------------
;*  File name:  boot.s
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 10-05-2020
;*--------------------------------------------------------------------------
;Este arquivo possui as rotinas de boot do SO
;--------------------------------------------------------------------------*/
%include "sys.inc"


global p4_table, ist_stack_1, p1_table_temp
global gdt64, gdt_size, gdt64.data, gdt64.code, gdt64.tss
global tss64
global _kernel_stack_bottom
global _start
global gdt64.code_tmp
global _GDT_TSS_SEL
global _GDT_USER_CODE32_SEL

global _GDT_KERNEL_CODE_SEL
global _GDT_KERNEL_DATA_SEL
global _GDT_USER_CODE_SEL
global _GDT_USER_DATA_SEL


;****************************************************************************
;                   ESTRUTURAS DE DADOS DO SISTEMA
;****************************************************************************
section .data
[bits 32]
align 16

;-------------------------------------------------
;             TSS
;-------------------------------------------------
tss64:
dd 0
times 3 dq 0 ; RSPn
dq 0 ; Reserved
.ist:
dq ist_stack_1 ; IST1, NMI
dq ist_stack_2 ; IST2, Double fault
dq ist_stack_3 ; IST3
dq 0 ; IST4
dq 0 ; IST6
dq 0 ; IST7
dq 0 ; Reserved
dw 0 ; Reserved
dw 0 ; I/O Map Base Address
tss_size equ $ - tss64 - 1

;-------------------------------------------------
;        GDT - Global Descriptor Table (64-bit)
;-------------------------------------------------
gdt64:   
.null equ $ - gdt64          ; The null descriptor
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 0                         ; Access
db 0                         ; Granularity
db 0                         ; Base (high)
.code equ $ - gdt64          ; The code descriptor
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 10011010b                 ; Access (exec/read)
db 00100000b                 ; Granularity
db 0                         ; Base (high)
.data equ $ - gdt64          ; The data descriptor
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 10010010b                 ; Access (read/write)
db 00000000b                 ; Granularity
db 0                         ; Base (high)
.user_code32 equ $ - gdt64   ; Ring 3 code descriptor (32-bit)
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 0                         ; Access (exec/read)
db 0                         ; Granularity
db 0                         ; Base (high)
.user_data equ $ - gdt64     ; Ring 3 data descriptor
dw 0xffff                        ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 11110010b                 ; Access (read/write)
;db 00000000b                 ; Granularity
db 11001111b                 ; Granularity
db 0                         ; Base (high)
.user_code64 equ $ - gdt64   ; Ring 3 code descriptor (64-bit)
dw 0xffff                    ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 11111010b                 ; Access (exec/read)
;db 00100000b                 ; Granularity
db 10101111b                 ; Granularity
db 0                         ; Base (high)
.tss equ $ - gdt64           ; The TSS descriptor
dw tss_size & 0xFFFF         ; Limit
dw 0                         ; Base (bytes 0-2)
db 0                         ; Base (byte 3)
db 10001001b                 ; Type, present
db 00000000b                 ; Misc
db 0                         ; Base (byte 4)
dd 0                         ; Base (bytes 5-8)
dd 0                         ; Zero / reserved
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Excluir a partir daqui. Foi criada apenas para testes
.code_tmp equ $ - gdt64          ; The code descriptor
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 10011010b                 ; Access (exec/read)
db 00100000b                 ; Granularity
db 0        
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;calculo a tribuo o limite da GDT ao rótulo "gdt_size".
;o limite da GDT é basicamente o seu tamanho em bytes, 
;subtraído de 1.
gdt_size: equ $ - gdt64 - 1
;---------------------------------------
; Em 32 bits os endereços não podem ser 64 bits
;---------------------------------------
gdt32_pointer: equ $ - KERNEL_TEXT_BASE
	dw gdt_size
	dq (gdt64 - KERNEL_TEXT_BASE)
;-------------------------------------------
;  Em 64 bits utilizamos esta
;-------------------------------------------
gdt64_pointer:
	dw gdt_size
	dq gdt64

;-------------------------------------------
; SELETORES(OFFSET) DOS DESCRITORES DE SEGMENTO
;-------------------------------------------

_GDT_TSS_SEL: dw gdt64.tss
; Kernel mode
_GDT_KERNEL_CODE_SEL: dw gdt64.code
_GDT_KERNEL_DATA_SEL: dw gdt64.data
; User mode
_GDT_USER_CODE_SEL: dw gdt64.user_code64
_GDT_USER_DATA_SEL: dw gdt64.user_data

; User mode - 32 bits
_GDT_USER_CODE32_SEL: dw gdt64.user_code32

;----------------------------------------------------
;                   CÓDIGO DO SISTEMA
;----------------------------------------------------
section .text
_start:
	; Disable interrupts
	cli

	; Clear the direction flag
	cld
	
	; Set stack pointer
	mov esp, _kernel_stack_bottom - KERNEL_TEXT_BASE

	; Clear screen
	mov ecx, 2000 ; 80 * 25 characters in the buffer
	jmp .cls_end

	.cls:
	dec ecx
	mov word [VGABUF + ecx * 2], 0x0F20

.cls_end:
	cmp ecx, 0
	jne .cls
	
	; Validate multiboot
	cmp eax, 0x36D76289
	jne .no_multiboot

	; Check 8-byte alignment of pointer
	mov ecx, ebx
	and ecx, (8 - 1)
	test ecx, ecx
	jnz .no_multiboot

	;***  VIDEO ***********
	; Set the cursor to blink
	outb 0x3D4, 0x09
	outb 0x3D5, 0x0F
	outb 0x3D4, 0x0B
	outb 0x3D5, 0x0F
	outb 0x3D4, 0x0A
	outb 0x3D5, 0x0E

	; Sets the cursor position to 0,1
	outb 0x3D4, 0x0F

	; Set cursor position to (row * 80) + col = (1 * 80) + 0 = 80
	outb 0x3D5, 80
	outb 0x3D4, 0x0E
	outb 0x3D5, 0x00
	;*************************************	

	; Check if CPUID is supported
	;-------------------------------------------
 	pushfd
 	pop eax
 	mov ecx, eax ; Save for later on
	xor eax, 1 << 21 ; Flip the ID bit
	push eax
	popfd

	; Put the old value back
	pushfd
	mov [esp], ecx
	popfd

	; See if it worked
	xor eax, ecx
	jz .no_cpuid
	;-------------------------------------------
	; Preserve multiboot information
	push ebx

	; Check if long mode is supported
	;-------------------------------------------
	mov eax, 0x80000000
	cpuid
	cmp eax, 0x80000001
	jb .no_long_mode

	mov eax, 0x80000001
	cpuid
	test edx, 1 << 29
	jz .no_long_mode
	;-------------------------------------------
	; Pop multiboot information
	pop ebx

	; Enable SSE (TODO: Do this in cpu/simd.asm)
	;-------------------------------------------
	mov eax, cr0
	and eax, 0xFFFB
	or ax, 2
	mov cr0, eax
	mov eax, cr4
	or ax, 3 << 9
	mov cr4, eax

	;------------------------------------------
	; Configura o Page Tables
	;------------------------------------------
	map_page p4_table, 511, p3_table
	map_page p3_table, 510, p2_table		
	;---- Recursive
	;map_page p4_table, 510, p4_table

	;-----------------------------------------
	map_page p2_table, 0, p1_table_0
	map_page p2_table, 1, p1_table_1
	map_page p2_table, 2, p1_table_2
	map_page p2_table, 3, p1_table_3
	map_page p2_table, 4, p1_table_4
	map_page p2_table, 5, p1_table_5
	map_page p2_table, 6, p1_table_6
	map_page p2_table, 7, p1_table_7
	
	; Map addresses from 0xFFFF800000000000
	;map_page p4_table, 256, lower_p3_table
	;map_page lower_p3_table, 0, p2_table
	map_page p4_table, 256, p3_table
	map_page p3_table, 0, p2_table

	; Map in some more memory around where we are so we have time to jump
	;-------------------------------------------------------------------
	;Faz o mapeamento identit da memória, o que permite a transição.
	;-----------------------------------------------------------------
	map_page p4_table, 0, p3_table_temp
	map_page p3_table_temp, 0, p2_table
	;-----------------------------------------------------------------
	; Map each P1 entry from 0 - 2MB
	; Set permissions for each section
	; section .text:   r-x
	; section .rodata: r--
	; section .data:   rw-
	; section .bss:    rw-
	mov ecx, 0

.map_p1_table:	
	mov eax, 4096
	mul ecx
	
	mov edi, eax
	mov esi, 0
	mov edx, edi
	within_labels _text_begin_phys, _text_end_phys
	test al, al
	jnz .is_text
	mov edx, edi
	within_labels _rodata_begin_phys, _rodata_end_phys
	test al, al
	jnz .is_rodata
.is_none: ; data, bss
	or edi, 1 << 1
	or esi, 1 << 31
	jmp .end
.is_text:
	and esi, ~(1 << 31) & 0xFFFFFFFF
	and edi, ~(1 << 1)
	jmp .end
.is_rodata: ; rodata
	and edi, ~(1 << 1)
	or esi, 1 << 31
.end:
	or edi, 0b100000001 ; Global, Present
	and edi, ~(1 << 2)  ; Supervisor only
	mov edx, p1_table_0
	
	mov [edx + ecx * 8], edi ; Low 4 bytes
	add edx, 4
	mov [edx + ecx * 8], esi ; High 4 bytes
	inc ecx
	cmp ecx, 512 * 8
	
	jne .map_p1_table

	; Make sure we allocated enough memory in the page tables for the kernel
	extern _kernel_end_phys
	mov eax, _kernel_end_phys
	cmp eax, KERNEL_PHYS_MAP_END
	jg .kernel_too_big

	;-----------------------------
	; Carrega P4 no registro CR3
	;-----------------------------
	mov eax, p4_table
	mov cr3, eax

	;----------------------------------------------------
	; Enable CR4.PAE(5)=1, global e CR4_PGE(7)=1
	;-----------------------------------------------------
	mov eax, cr4
	or eax, (1 << CR4_PAE) | (1 << CR4_PGE)
	mov cr4, eax

	;------------------------------------------------------------------------------
	;Habilito o LONG MODE EFER.LME=1, utilizando o  MSR código=0xC0000080(o código do MSR
	;a ser inserido em ECX) para uso com RDMSR)
	;----------------------------------------------------------------------------------
	mov ecx, 0xC0000080 ; EFER
	rdmsr
	or eax, (1 << 8) | (1 << 11) ; Long mode, NX
	wrmsr

	;----------------------------------------------------------------------------------
	;Habilitamos a Paginação para possibilitar a ativação do Long Mode: CR0.PG=1.
	;Com a gravação do CR0, automaticamente é modificado o flag LMA do IA32_EFER.
	;Enable paging, pmode, write protect	
	;----------------------------------------------------------------------------------	
	mov eax, cr0
	mov eax, (1 << 0) | (1 << 31) | (1 << 16)
	mov cr0, eax	

	;----------------------------------------------------------------------------------	
	;Para ingressarmos no long mode, precisamo realiZar um salto(far jump). A seguir, 
	;fazemos um salto para serializar o processador, carregando o registro CS com a 
	;indicação do descritor de segmento 0x08: indica o índice do descptor no GDT.
	;Mas antes, é preciso inserir a GDT para 64 bits. Com esse salto, já estamos no 
	;LONG MODE e em 64 bits. Tive muitos problemas.
	;------------------------------------------------------------------------------------		
	lgdt [gdt32_pointer]
	jmp gdt64.code:long_mode

; Multiboot magic value was incorrect
.no_multiboot:
	mov al, "0"
	jmp .error

; Processor doesn't support CPUID instruction
.no_cpuid:
	mov al, "1"
	jmp .error

; Processor doesn't support long mode
.no_long_mode:
	mov al, "2"
	jmp .error

; Kernel executable file is greater than the memory we allocated for page tables
.kernel_too_big:
	mov al, "3"
	jmp .error

; .error: Writes "ERROR: $al" in red text and halts the CPU
; al: Contains the ASCII character to be printed to the screen
.error:
	mov dword [VGABUF], 0x4F524F45
	mov dword [VGABUF + 4], 0x4F4F4F52
	mov dword [VGABUF + 8], 0x4F3A4F52
	mov dword [VGABUF + 12], 0x4F204F20
	mov byte  [VGABUF + 14], al
	hlt
;-------------------------------------------------------------------------
[bits 64]
long_mode: equ $ - KERNEL_TEXT_BASE
;-------------------------------------------------------------------------
	;-----------------------------
	; Load TSS descriptor into GDT
	;-----------------------------	
	mov rdi, gdt64
	add rdi, gdt64.tss
	mov rax, tss64
	mov word [rdi + 2], ax
	shr rax, 16
	mov byte [rdi + 4], al
	shr rax, 8
	mov byte [rdi + 7], al
	shr rax, 8
	mov dword [rdi + 8], eax
	;-------------------------------
	; Load GDT
	;-------------------------------
	;Aqui ajustamos o ponteiro da stack(RSP) para a posição equivalente
	;no endereço virtual(0xFFFFFFFF80000000)
	mov rax, KERNEL_TEXT_BASE
	add rsp, rax	
	;-------------------------------------
	; Carrego o ponteiro para a GDT com valores em 64 bits.
	;-------------------------------------
	lgdt [gdt64_pointer]
	mov rax, higher_half
	jmp rax
	
higher_half:
	; Unmap the lower half page tables
	mov rax, p4_table  ; move o endereço físico para RAX
	mov rdi, KERNEL_TEXT_BASE ; soma e encontra o endereço virtual da PML4
	add rax, rdi ; RDI tem o endereço do primeiro registro de PML4	
	mov qword [rax], 0	; Zera o primeiro registro de PML4
	
	; Align RSP again
	add rsp, 10

	; Flush whole TLB
	mov rax, cr3
	mov cr3, rax

;********************* incluído em 28/06/2020  
	extern long_mode_entry
	mov rax, long_mode_entry
	jmp rax
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                     SECTION .BSS
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;As estruturas do Page Tables e das Stack serão criadas/reservadas na section .BSS.
;Inciciamos reservando os espaços do Page Tables. A criação dos Page Tables exige o 
;uso dos endereços físicos de cada tabela, iniciando com a PML4(p4_table), cujo ende-
;ço físico deve ser carregado do registro CR3.
;Para obtermos o endereço físico, subtraímos KERNEL_TEXT_BASE(0xFFFFFFFF80000000) do
;endereço virtual de cada tabela($).
;***********************************************************************************
section .bss

;----------------------------------------------------------
;          PAGE TABLES
;----------------------------------------------------------

align PAGE_SIZE

p4_table equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p3_table equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

lower_p3_table equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p2_table equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p1_table_0 equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p1_table_1 equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p1_table_2 equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p1_table_3 equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p1_table_4 equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p1_table_5 equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p1_table_6 equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p1_table_7 equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

;NÃO EXCLUIR: Esta tabela será utilizada para mapeamento temporário(vmm.h:
;void create_temporary_pagetable(void)).
p1_table_temp equ $ - KERNEL_TEXT_BASE
resb PAGE_SIZE

p3_table_temp equ $ - KERNEL_TEXT_BASE ; Use the top of the stack temporarily

;--------------------------------------------
;            Stack do Kernel(16 kb)
;--------------------------------------------
_kernel_stack_top:
resb PAGE_SIZE * 4 ; 16kB
_kernel_stack_bottom:

;------------------------------------------
;            Stacks do TSS->IST
;------------------------------------------

;Stacks for the Interrupt Stack Table - IST
resb PAGE_SIZE
ist_stack_1 equ $ 
resb PAGE_SIZE
ist_stack_2 equ $ 
resb PAGE_SIZE
ist_stack_3 equ $ 
resb PAGE_SIZE
;----------------------- END - STACK'S DO SISTEMA ---------------------------
