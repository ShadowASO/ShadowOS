;/*--------------------------------------------------------------------------
;*  File name:  vesa_tramp.s
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
;considera que é um opcde errado. Resolvi utilizando um far jump com JMP:
;jmp 0x0:LABEL_OFFSET(videodone).
;Solução: Acabei descobrindo como resolver o problema do command JE. Estou 
;utilizando a macro NEAR_OFFSET(label)   (label-($+2)). Ela usa um near jump
;offset. O cálculo é sempre feito a partir da instrução que se segue ao 
;instrução [JE operand]. Por isso, tenho que somar dois bytes. Esse valor é
;3 bytes quando usamos o [jmp operand]
;SHORT E NEAR JUMP: Depois de muito tempo perdido, percebi que os saltos(jump)
;near e short não envolviam endereços de memória, mas deslocamentos relativos 
;à instrução que se segue ao comando jmp ou je. Assim, não tem problema que o
;fragmento de código tenha sido copiado de outro espaço de endereçamento. A
;quantidade de bytes dentro do código não se altera.

;------------------------------------------------------------------------
;Esta função faz um salto direto do real mode para o long mode, o que pare-
;ce contrariar a informação de que é preciso estar no modo protegido para
;ir ao long mode.
;--------------------------------------------------------------------------*/
%include "sys.inc"

[bits 16]
%define LOAD_ADDR 0x1000
%define TMP_DATA_OFFSET 0x0A00
%define DATA_OFFSET (LOAD_ADDR + TMP_DATA_OFFSET + ($ - vesa_dados_virtuais))
%define LABEL_OFFSET(label) (LOAD_ADDR + ((label) - vesa_realmode_start))
;Exclusivo para jump condicionais(2 bytes)
%define NEAR_OFFSET(label)   (label-($+2)) 

;--------------------------------------------------------------------------
align PAGE_SIZE
global vesa_realmode_start
vesa_realmode_start: equ $
align 16
	cli
	;lidt [tmp_idt_ptr_null]
	lidt[idt_real]
	; Setup segments
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
	;mov esp, tmp_stack_bottom	
;---------------------------------------------------------------
;Desativo o cursor	
;----------------------------------------------------------------
	mov	ah, 1
	mov	cl, 0
	mov	ch, 0x20
	int	0x10
;----------------------------------------------------------------
;# Now, set the video mode using VBE interrupts.
;# Keep trying until we find one that works.

;# These are documented on page 30 of the VESA-BIOS manual:
;# interrupt 0x10
;# ax = 0x4f02 "Set VBE Mode"
;# bx = mode
;#    D0-8   = Mode Number
;#    D9-10  = Reserved (must be 0)
;#    D11    = 0 Use current default refresh rate.
;#    D12-13 = 0 Reserved
;#    D14    = 0 Use windowed frame buffer model.
;#           = 1 Use linear frame buffer model.
;#    D15    = 0 Clear display memory.
;#    ES:DI  = Pointer to CRCTCInfoBlock structure.
;-----------------------------------------------------------------
;jmp videodone
jmp .video800

.video1280: equ $
 	mov	ax, 0x4f02
 	mov	bx,0x411B
	int	0x10
	cmp	ax,0x004f		
	je .videodone
.video1024: equ $
 	mov	ax,0x4f02
 	mov	bx,0x4118
	int	0x10
	cmp	ax,0x004f	
	je .videodone
.video800: equ $
	mov	ax,0x4f02
 	mov	bx,0x4115
	int	0x10
	cmp	ax,0x004f
	je .videodone	
.video640: equ $
	mov	ax, 0x4f02
 	mov	bx, 0x4112
	int	0x10
	cmp	ax, 0x004f	
	je .videodone
.video640_lowcolor: equ $
	mov	ax,0x4f02
	mov	bx,0x4111
	int	0x10
	cmp	ax,0x004f	
	je .videodone
;-----------------------------------------------------------------
.videodone: 
	
;-----------------------------------------------------------------
;Extraio as informações do modo de vídeo e as guardo na estrutura 
;"video_vbemode", criado no cabeçalho do código. Ela precisa ficar
;no início para facilitar a sua leitura a partir de x64 bits.
;-----------------------------------------------------------------		
	mov di,vbe_info
	mov	ax, 0x4f00
	mov	cx, bx
	int	0x10

	mov di,vbe_mode_info
	mov	ax, 0x4f01
	mov	cx, bx
	int	0x10

	;Atributo testado para verificar a conclusão da operação	
	mov word[video_mode], bx
	
;---------------------------------------------------------------
vesa_end:
.halt:
    cli
	pause
    hlt
    jmp .halt

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;         ESTRUTURA TEMPORÁRIAS
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
times TMP_DATA_OFFSET - ($ - vesa_realmode_start) db 0
vesa_dados_virtuais:

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;         ESTRUTURAS DE DADOS DO VBE
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
align 16
vbe_info: equ DATA_OFFSET
		times 512 db 0
align 16
vbe_mode_info: equ DATA_OFFSET
		times 256 db 0

align 16
video_mode: equ DATA_OFFSET
		times 2 db 0

idt_real: equ DATA_OFFSET
	dw 0x3ff		; 256 entries, 4b each = 1K
	dd 0			; Real Mode IVT @ 0x0000

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;    ROTINAS PARA MANIPULAR OS DADOS DE ESTRUTURA DO VBE
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
[bits 64]
;------------------------------------------
;void *_get_vbe_info(void)
;------------------------------------------
global _get_vbe_info:
_get_vbe_info:
	mov rax, vbe_info
	ret
;------------------------------------------
;void *_get_vbe_mode_info(void)
;------------------------------------------
global _get_vbe_mode_info:
_get_vbe_mode_info:
	mov rax, vbe_mode_info
	ret
;------------------------------------------
;------------------------------------------
;u16_t _get_video_status(void)
;------------------------------------------
global _get_video_status:
_get_video_status:
	mov ax, word[video_mode]
	ret

global vesa_realmode_end
vesa_realmode_end: equ $