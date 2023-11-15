;/*--------------------------------------------------------------------------
;*  File name:  x86_64.s
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 01-08-2020
;*--------------------------------------------------------------------------
;Este arquivo fonte possui diversos rotinas escritas em assembly para manipular
;diretamente o processador
;Todas as rotinas precisam ter um ret
;--------------------------------------------------------------------------*/
%include "sys.inc"

extern  gdt64.code, gdt64.data  

[bits 64]
align 16
;---------------------------------------------------------------------------
;    ESTRUTURAS DE DADOS
;---------------------------------------------------------------------------
section .data
;----------------------------------------------
; Estrutura para far jump
;----------------------------------------------
_far_ptr_:
	dq 0
	dw 0   

alinha0:
    dw 0
    dd 0    
;------------------------------------------
; Estrutura para ponteiro de Table descriptor
;------------------------------------------
_table_ptr_:
	dw 0
	dq 0

;---------------------------------------------------------------------------
;    CÓDIGO DAS ROTINAS
;---------------------------------------------------------------------------
section .text
align 16

;---------------------------------------------------------------------------
;u64_t __read_cr2(void)
;Devolve o conteúdo do registro cr2
;---------------------------------------------------------------------------
global __read_cr2
__read_cr2:
    mov rax, cr2
    ret

;---------------------------------------------------------------------------
;u64_t __read_cr3(void)
;Devolve o endereço de PML4 no registro cr3
;---------------------------------------------------------------------------
global __read_cr3
__read_cr3:
    mov rax, cr3
    ret

;---------------------------------------------------------------------------
;void __write_cr3(u64_t cr3)
;Grava o endereço físico do pagetable PML4 no registro cr3
;---------------------------------------------------------------------------
global __write_cr3
__write_cr3:
    mov cr3, rdi    
    ret

;---------------------------------------------------------------------------
;u64_t __read_cr4(void)
; Retorna o endereço em cr3
;---------------------------------------------------------------------------
global __read_cr4
__read_cr4:
    mov rax, cr4
    ret

;---------------------------------------------------------------------------
;Ler o RIP na atual posição do código
;u64_t __read_rip(void) 
;---------------------------------------------------------------------------
global __read_rip
__read_rip:    
    pop rax                     ; Get the return address
    jmp rax                     ; Return. Can't use RET because return
                                ; address popped off the stack. x
;---------------------------------------------------------------------------
;void __write_lgdt(u64_t gdt_ptr);
;carrega uma nova GDT e faz um salto "far jump". Sem esse salto, gera um 
;erro intermitente.
;--------------------------------------------------------------------------
global __write_lgdt
__write_lgdt:
    cli
    lgdt [RDI]

    ;realiza um far jump para atualizar o CS
    lea rax, [rel .far_ptr]			
	jmp far [rax]	
;------------------------------------
.far_ptr:
	dq .fix_cs
	dw gdt64.code   
;-------------------------------------
.fix_cs:
    sti
    ret

;------------------------------------------------
;void __load_gdt(u64_t gdt_addr, u16_t code_sel);
;extern void __load_gdt(u64_t gdt_addr, u16_t gdt_size, u16_t selector);
;------------------------------------------------
;RDI=gdt_addr
;RSI=gdt_size
;RDX=selector
;-----------------------------------------------
;Carrega um nova GDT e faz a serialização com um far jump
;------------------------------------------------
global __load_gdt
__load_gdt:
    cli
    ;preencho a estrutura do ponteiro e carrego no registro.
    xor rax, rax
    lea rax, [_table_ptr_]
    mov word [rax], si
    mov qword[rax + 2], rdi
    lgdt [rax]

    ;lgdt [RDI]

    ;Utilizamos a estrutura "far_ptr", criado na seção .data.
    ;preencho a estrutura "far_ptr"
    xor rax, rax
    lea rax, [_far_ptr_]    
    mov qword[rax], .fix_cs    
    mov word[rax + 8], dx; si
    ;----------------------------------------------------------   
    ;faz o salto 
	jmp far [rax]	

.fix_cs:
    sti
    ret

;---------------------------------------------------------------------------

global __write_lidt
__write_lidt:
    cli
    mov rax, rdi
    lidt [rax]
    sti
    ret

;------------------------------------------------
;extern void __load_idt(u64_t idt_addr, u16_t idt_size);
;------------------------------------------------
;RDI=idt_addr
;RSI=idt_size
;
;-----------------------------------------------
;Carrega um nova IDT.
;------------------------------------------------
global __load_idt
__load_idt:
    cli

    ;preencho a estrutura do ponteiro e carrego no registro.
    xor rax, rax
    lea rax, [_table_ptr_]
    mov word [rax], si
    mov qword[rax + 2], rdi
    lidt [rax]      

    sti
    ret

;---------------------------------------------------------
; void __load_tss(u16_t selector)
;--------------------------------------------------------
;RDI = selector
;--------------------------------------------------------
; Carrega o TSS a partir da entrada na GDT
;---------------------------------------------------------
global __load_tss
__load_tss:		
	cli

	mov ax, di
	ltr ax	

	sti
	ret
;------------------------------------------------------------------------------

;---------------------------------------------------------------------------

global __enable_irq
__enable_irq:
    sti
    ret

;---------------------------------------------------------------------------
global __disable_irq
__disable_irq:
    cli
    ret

;---------------------------------------------------------------------------
;Provoca uma interrupção de tempo e pode ser utilizada para gerar uma
;troca de contexto
global __int32  
__int32:   
    int 32    
    ret
;---------------------------------------------------------------------------
global __int80
__int80:
    int 0x80        
    ret
;---------------------------------------------------------------------------

;---------------------------------------------------------------------------
global __wait
__wait:
    pause
    hlt 
    ret ; a ausência desse ret causou muito problema nas rotinas de interrupção

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Em essência, cuida-se de um while com o command pause, para poupar recursos da
; CPU.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
global __busy_wait
__busy_wait:
    .halt: 
        sti       
        pause
        hlt 
        jmp .halt
    ret ; a ausência desse ret causou muito problema nas rotinas de interrupção
;---------------------------------------------------------------------------
;---------------------------------------------------------------------------
global __stop
__stop:
    .halt:
        cli
        pause
        hlt 
        jmp .halt
    ret ; a ausência desse ret causou muito problema nas rotinas de interrupção
;---------------------------------------------------------------------------

;*****************************************************************************************
;Verifica o CPUID
;__get_cpuid(u64_t eax, u64_t *ebx, u64_t *ecx, u64_t *edx)
;*****************************************************************************************
;eax=rdi
;ebx=rsi
;ecx=rdx
;edx=rcx
;****************************************************************************************
global __get_cpuid
__get_cpuid:
    push rbp          ;salvo na pilha a antiga base da pilha
    mov rbp, rsp     ;torno a base da pilha igual ao topo da pilha
    ;---------------------------------------------------------------------   
    push rcx
    push rdx   
    
    mov eax, [rdi]
	cpuid

    pop r10 ; rdx=ecx
    pop r11 ; rcx=edx
   
    mov [rdi], eax
    mov [rsi], ebx
    mov [r10], ecx
    mov [r11], edx
   
	.return:     
    mov rsp, rbp
    pop rbp
    ret

;---------------------------------------------------------------------------
;Desabilita o PIC
global __disable_pic
__disable_pic:
    mov al, 0xff
    out 0xa1, al
    out 0x21, al
    ret

;---------------------------------------------------------------------------
;ShutDown o computer
shutdown:
  mov ax, 0x1000
  mov ax, ss
  mov sp, 0xf000
  mov ax, 0x5307
  mov bx, 0x0001
  mov cx, 0x0003
  int 0x15 
  ret  

;---------------------------------------------------------------------------
;u64_t __read_cpu_msr(u64_t msr_id, u64_t *msr_low, u64_t *msr_high)
;---------------------------------------------------------------------------
;msr_id = rdi
;msr_low=rsi
;msr_high=rdx
;---------------------------------------------------------------------------
global __read_cpu_msr
__read_cpu_msr:
    push rbp          ;salvo na pilha a antiga base da pilha
    mov rbp, rsp     ;torno a base da pilha igual ao topo da pilha
    ;---------------------------------------------------------------------       
    ;push rsi    
    ;push rdx 
    ;Transfiro o código do MSR desejado para RCX
    mov ecx, edi
    mov r10, rsi ; low
    mov r11, rdx ; high    
	rdmsr
       
    mov [r10], eax ;rdx
    mov [r11], edx ;rax
    
	.return:     
    mov rsp, rbp
    pop rbp
    ret

;---------------------------------------------------------------------------
;__write_cpu_msr(u64_t msr_id, u64_t msr_low, u64_t msr_high)
;---------------------------------------------------------------------------
;RDI=msr_id
;RSI=msr_low
;RDX=msr_high
;---------------------------------------------------------------------------
global __write_cpu_msr
__write_cpu_msr:
    push rbp          ;salvo na pilha a antiga base da pilha
    mov rbp, rsp     ;torno a base da pilha igual ao topo da pilha
    ;---------------------------------------------------------------------     
    ;Transfiro o código do MSR desejado para ecx
    mov ecx, edi
    mov eax, esi
	wrmsr
   
	.return:     
    mov rsp, rbp
    pop rbp
    ret

;---------------------------------------------------------------------------
;void __enable_nxe_bit(void)
; Aplica o endereço em cr3
;---------------------------------------------------------------------------
global __enable_nxe_bit
__enable_nxe_bit:
    ; Set long mode, NXE bit
	mov ecx, 0xC0000080
	rdmsr
	or eax, (1 << 8) | (1 << 11)
	wrmsr

;---------------------------------------------------------------------------
;Rotina para fazer a invalidação de toda a TLB
;void __vm_flush_tlb(void)
;---------------------------------------------------------------------------
; Aplica o endereço em cr3
;---------------------------------------------------------------------------
global __vm_flush_tlb
__vm_flush_tlb:
    mov rax, cr3   
    mov cr3, rax 
    ret

;---------------------------------------------------------------------------
;Rotina para fazer a invalidação da entrada na TLB de uma determinada tabela da 
;estrutura de paginação(Page Tables). Recebe o endereço virtual da tabela.
;void __vm_invlpg_tlb(u64_t v_addr)
;---------------------------------------------------------------------------
; Aplica o endereço em cr3
;---------------------------------------------------------------------------
global __vm_invlpg_tlb
__vm_invlpg_tlb:
    invlpg [rdi]    
    ret

;---------------------------------------------------------------------------
;Ler o rflags 
;u64_t __read_rflags64(void) 
;---------------------------------------------------------------------------
global __read_rflags64
__read_rflags64:
   pushfq
   pop rax
   ret


;---------------------------------------------------------------------------
global __read_reg_rsp
__read_reg_rsp:
   mov rax, rsp
   ret
;---------------------------------------------------------------------------
;void  __set_stack_rsp(uint65_t virt_addrs)
;---------------------------------------------------------------------------
global __set_stack_rsp
__set_stack_rsp:
   mov rsp, rdi
   ret
;---------------------------------------------------------------------------
;---------------------------------------------------------------------------
;u64_t __read_tsc(void)
; Retorna counter do TSC
;---------------------------------------------------------------------------
global __read_tsc
__read_tsc:        
    lfence   
    rdtsc   
    lfence 
    ;rol rdx, 32
    shl rdx, 32
    or rax, rdx    
    ret


