;/*--------------------------------------------------------------------------
;*  File name:  tramp.s
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 19-03-2022
;*  Revisão: 25-11-2022
;*--------------------------------------------------------------------------
;Este arquivo possui o código para fazer a ativação dos núcleos AP do proces-
;sador MP. 
;É importante observar que todas as referêcnias a label devem ser realizadas
;utilizando a macro LABEL_OFFSET(label). Ela calcula o deslocamento do label
;em relação ao início do fragmento de código(smp_trampoline_start) e soma ao
;endereço onde o código do trampoline será copiado(LOAD_ADDR=0x1000).
;Tive muitos problemas opcode error 0x6 para utilizar a macro com o commando 
;JE LABEL_OFFSET(videodone). Aparentemente, em tempo de execução o sistema
;considera que é um opcode errado. Resolvi utilizando um far jump com JMP:
;jmp 0x0:LABEL_OFFSET(videodone).

;SHORT E NEAR JUMP: Depois de muito tempo perdido, percebi que os saltos(jump)
;near e short não envolviam endereços de memória, mas deslocamentos relativos 
;à instrução que se segue ao comando jmp ou je. Assim, não tem problema que o
;fragmento de código tenha sido copiado de outro espaço de endereçamento. A
;quantidade de bytes dentro do código não se altera.

;------------------------------------------------------------------------
;Esta função faz um salto direto do real mode para o long mode, o que pare-
;ce contrariar a informação de que é preciso estar no modo protegido para
;ir ao long mode.
;-------------------------------------------------------------------------
;--------------------------------------------------------------------------*/
%include "sys.inc"
%include "x86_64.inc"


%define LOAD_ADDR 0x1000
%define TMP_DATA_OFFSET 0x0A00
%define DATA_OFFSET (LOAD_ADDR + TMP_DATA_OFFSET + ($ - tmp_data_virt))
%define LABEL_OFFSET(label) (LOAD_ADDR + ((label) - smp_trampoline_start))

align PAGE_SIZE
global smp_trampoline_start
smp_trampoline_start equ $
tmp_code_virt:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                   START OF 16-BITS CODE
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;------------------------------------------------------------------------
;Aqui nós acabamos de sair da execução do código de inicialização contido na
;EPROM. A EPROM contém o código de inicialização e está localizado no ende-
;reço 0xFFFFFFF0. Neste momento, o processador está executando em real-mode
;e os registros apontam para valores e endereços relativos ao código da ini-
;cialização(EPROM).
;-------------------------------------------------------------------------
bits 16
ap_start16:
	cli
	lidt [tmp_idt_ptr_null]
	;Neste ponto, o base-address do CS(parte oculta), continua retendo o va-
	;lor 0xFFFF0000. Ou seja, o processador continuar executando código  da
	;EPROM. Para mudar isso, precisamos de um FAR JUMP ou CALL. O comando 
	;abairo carregara o base-address como 0x0 e EIP com o offset de ".fix_cs"
	jmp 0x0:LABEL_OFFSET(.fix_cs)
	

.fix_cs:
	xor eax, eax
	xor ebx, ebx
	xor ecx, ecx
	xor edx, edx
	xor esi, esi
	xor edi, edi
	xor ebp, ebp
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax
	
	;----------------------------------------------------------------------------
	;Atribuo a stack temporária para uso nesta fase. Existe uma única stack que é
	;utilizada em todas as fases da inicialização, começando em 16 bits até 64 bits,
	;quando é substituída pela definitiva, criada em smp.c.
	;----------------------------------------------------------------------------
	mov esp, tmp_stack_bottom	

	; Enable CR4.PAE(5)=1, global-page CR4.PGE(7)=1
	; Physical-Address Extensions(PAE) deve ser habilitada antes de entrar no modo
	; longo. Ele transforma toda a paginação em 64 bits.
	;-----------------------------------------------------
	xor eax, eax
	mov eax, cr4
	or eax, (1 << 5) | (1 << 7)
	mov cr4, eax
	;-----------------------------------------------------

;%comment 

	;Carrego o endereço do page table PML4 no CR3(coloco o endereço absoluto porque dava erro)
	;Utilizo o mesmo Page Table do BSP. O page table deve estar inicialmente abaixo 
	;de 4Gb, ou o endereço não poderá ser manipulado em 32 bits.
	;Posteriormente, substitui o valor absoluto pela constante criada no script do link.
	;A valor dessas constantes pode ser obtido em assembly diretamente do nome, sem
	;precisar fazer referência a endereço de memória. Em "C", exige "&": &_swapper_pt4_dir.
	;Sem a indicação prévia do endereço físico da PML4, ocorre um erro quando habilitamos
	;o paging.
;%endcomment
	;---------------------------------------------------------------------------------	
	
	extern _swapper_pt4_dir
	mov eax,_swapper_pt4_dir
	mov cr3, eax

	;------------------------------------------------------------------------------
	;Habilito o LONG MODE EFER.LME=1, utilizando o  MSR código=0xC0000080(o código do MSR
	;a ser inserido em ECX) para uso com RDMSR)
	;----------------------------------------------------------------------------------
	mov ecx, 0xC0000080 ; EFER
	rdmsr
	or eax, (1 << 8) | (1 << 11) | (1 << 0); Long mode, NX
	wrmsr

	;----------------------------------------------------------------------------------
	;Habilitamos a Paginação para possibilitar a ativação do Long Mode: CR0.PG=1.
	;Com a gravação do CR0, automaticamente é modificado o flag LMA do IA32_EFER.
	;Enable paging(PG), pmode(PE), write protect(WP)	
	;----------------------------------------------------------------------------------
	mov eax, cr0
	;mov eax, (1 << CR0_PE) | (1 << CR0_PG) | (1 << CR0_WP) | (1 << CR0_MP) | (1 << CR0_ET) | (1 << CR0_NE) | (1 << CR0_AM)
	or eax,  (1 << CR0_PE) ;/* Enable protected mode */
	or eax,  (1 << CR0_PG) ;/* Enable paging and in turn activate Long Mode */
	or eax,  (1 << CR0_WP) ;/* Enable Write Protect */
	
	mov cr0, eax

	;---------------------------------------------------------------------------------
	;Para ingressarmos no long mode, precisamo realiZar um salto(far jump). A seguir, 
	;fazemos um salto para serializar o processador, carregando o registro CS com a 
	;indicação do descritor de segmento 0x08: indica o índice do descptor no GDT.
	;Mas antes, é preciso inserir a GDT para 64 bits. Com esse salto, já estamos no 
	;LONG MODE e em 64 bits. Tive muitos problemas.
	;------------------------------------------------------------------------------------
	lgdt [tmp_gdt_ptr]	
	jmp 0x08:LABEL_OFFSET(ap_start64)
	
	;------------------------------------------------------------------------------------

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;               START OF 64-BITS CODE
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
[bits 64]

extern gdt_size
extern gdt64
extern gdt64.code
ap_start64:
	;/* Reload segment caches */
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	

	lea rax, [tmp_gdt_ptr]
	mov word [rax], gdt_size
	mov qword [rax + 2], gdt64
	lgdt [rax]

	; Restore it to its previous state for the next AP
	mov word [rax], (tmp_gdt_end - tmp_gdt - 1)
	mov qword [rax + 2], tmp_gdt

	;-----------------------------------------
	;Usando uma macro para fazer um far jump
	RELOAD_CS GDT_KERNEL_CODE ; gdt64.code
	;----------------------------------------

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Estrutura ".far_ptr" utilizada com "jump far indireto". Passamos o
;endereço inicial na memória e o sistema faz a atribuição CS:RIP.
;-----------------------------------			
;	lea rax, [rel .far_ptr]			
;	jmp far [rax]	
;------------------------------------
;.far_ptr:	
;	dq  .fix_cs
;	dw gdt64.code
;-------------------------------------
;.fix_cs:	
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	xor ax, ax
	mov ax, GDT_KERNEL_DATA
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	
	;Carrego uma stack criada em smp->smp_boot_ap
	extern smp_ap_stack	
	mov rsp, [smp_ap_stack]

	;Siinaliza de volta ao BSP se o core foi inicializado.						
	extern smp_ap_started_flag
	lock inc byte [smp_ap_started_flag]
		
	;;;;;;;;;;;;;;;;;; ATENÇÃO ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;O uso de um registro(RAX) intermediário se mostrou necessário
	;para utilizar a instruction CALL. Tudo indica que o tipo
	;de chamada(NEAR/FAR) é determinada pelo registro. Quando
	;fazemos a chamada direta, o sistema gera sempre um erro.
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	extern percpu_init_ap	
	mov rax, percpu_init_ap
	call rax	
						
	extern smp_ap_kmain	
	mov rax, smp_ap_kmain
	call rax
	
	;Se houver um retorno, entra em loop
	jmp _stop	

_stop:
	sti
	hlt
	pause
	jmp _stop

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                     ESTRUTURA TEMPORÁRIAS
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Todas as estruturas criadas aqui têm os seus label calculados como um offset inicia-
;do no ponto zero da memória: 0:offset.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
times TMP_DATA_OFFSET - ($ - smp_trampoline_start) db 0
tmp_data_virt:

;---------------------------------------
;                    GDT 
;---------------------------------------
align 16
tmp_gdt: equ DATA_OFFSET
	.null: equ $ - tmp_gdt   ; The null descriptor
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 0                         ; Access
db 0                         ; Granularity
db 0                         ; Base (high)
.code: equ $ - tmp_gdt          ; The code descriptor
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 10011010b                 ; Access (exec/read)
db 00100000b                 ; Granularity
db 0                         ; Base (high)
.data: equ $ - tmp_gdt          ; The data descriptor
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 10010010b                 ; Access (read/write)
db 00000000b                 ; Granularity
db 0                         ; Base (high)
.user_code32: equ $ - tmp_gdt   ; Ring 3 code descriptor (32-bit)
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 0                         ; Access (exec/read)
db 0                         ; Granularity
db 0                         ; Base (high)
.user_data: equ $ - tmp_gdt     ; Ring 3 data descriptor
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 11110010b                 ; Access (read/write)
db 00000000b                 ; Granularity
db 0                         ; Base (high)
.user_code64: equ $ - tmp_gdt   ; Ring 3 code descriptor (64-bit)
dw 0                         ; Limit (low)
dw 0                         ; Base (low)
db 0                         ; Base (middle)
db 11111010b                 ; Access (exec/read)
db 00100000b                 ; Granularity
db 0                         ; Base (high)
tmp_gdt_end: equ DATA_OFFSET

;------------------------------------------
;      PONTEIRO PARA GDT
;------------------------------------------
tmp_gdt_ptr: equ DATA_OFFSET
	dw (tmp_gdt_end - tmp_gdt - 1)
	dq tmp_gdt

;------------------------------------------
;      PONTEIRO NULO PARA IDT
;------------------------------------------
; Any NMI will shutdown, which is what we want
align 16
tmp_idt_ptr_null: equ DATA_OFFSET
	dw 0
	dq 0

;------------------------------------------
;           STACK PROVISÓRIA
;------------------------------------------
align 16
tmp_stack_top: equ DATA_OFFSET
	times 512 db 0
tmp_stack_bottom: equ DATA_OFFSET

;------------------------------------------
;           STACK PROVISÓRIA
;------------------------------------------
align 16
tmp_stack_top64: equ DATA_OFFSET
	times 512 db 0
tmp_stack_bottom64: equ DATA_OFFSET


global smp_trampoline_end
smp_trampoline_end equ $
