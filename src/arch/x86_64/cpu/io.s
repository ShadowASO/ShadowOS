;/*--------------------------------------------------------------------------
;*  File name:  io.asm
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 22-07-2020
;*--------------------------------------------------------------------------
;*  Rotina em assembly para manipular portas I/O.
;*--------------------------------------------------------------------------*/
%include "sys.inc"

;**************************************************************************
;void __write_portb(uint16_t port, uint8_t data);
;--------------------------------------------------------------------------
;Finalidade: É a rotina que manipula dados para as portas I/O
;--------------------------------------------------------------------------
;Parâmetros: 
;   1 - port(RDI);  
;   2 - data(RSI);
;--------------------------------------------------------------------------*/

global __write_portb
__write_portb:
    mov dx, di  ; indica o endereço da porta I/O
    mov al, sil ; move o valor a ser enviado à porta
    out dx, al  ; envia o dado para a porta I/O
    ret

;**************************************************************************
;uint8_t __read_portb(uint16_t port);
;--------------------------------------------------------------------------
;Finalidade: É a rotina que manipula dados para as portas I/O
;--------------------------------------------------------------------------
;Parâmetros: 
;   1 - port(RDI);  
;   
;--------------------------------------------------------------------------
;Retorna: void
;**************************************************************************
global __read_portb
__read_portb:
    mov dx, di  ; indica o endereço da porta I/O
    in al, dx  ; envia o dado da porta I/O para AL
    ret	
	
;**************************************************************************
;void __write_portw(uint16_t port, uint16_t data);
;--------------------------------------------------------------------------
;Finalidade: É a rotina que manipula dados para as portas I/O
;--------------------------------------------------------------------------
;Parâmetros: 
;   1 - port(RDI);  
;   2 - data(RSI);
;--------------------------------------------------------------------------
;Retorna: void
;**************************************************************************
global __write_portw
__write_portw:
    ;mov dx, di  ; indica o endereço da porta I/O
    ;mov ax, si ; move o valor a ser enviado à porta
    ;out dx, al  ; envia o dado para a porta I/O

    mov dx, di  ; indica o endereço da porta I/O
    mov ax, si ; move o valor a ser enviado à porta
    out dx, al  ; envia o dado para a porta I/O
    ret

;**************************************************************************
;uint16_t __read_portw(uint16_t port);
;--------------------------------------------------------------------------
;Finalidade: É a rotina que manipula dados para as portas I/O
;--------------------------------------------------------------------------
;Parâmetros: 
;   1 - port(RDI);  ;   
;--------------------------------------------------------------------------
;Retorna: void
;--------------------------------------------------------------------------
global __read_portw
__read_portw:
    mov dx, di  ; indica o endereço da porta I/O
    in ax, dx  ; envia o dado da porta I/O para AX
    ret	

;**************************************************************************	

;**************************************************************************
;void __write_portd(uint16_t port, uint32_t data);
;--------------------------------------------------------------------------
;Finalidade: É a rotina que manipula dados para as portas I/O
;--------------------------------------------------------------------------
;Parâmetros: 
;   1 - port(RDI);  
;   2 - data(RSI);
;--------------------------------------------------------------------------
;Retorna: void
;**************************************************************************
global __write_portd
__write_portd:
    ;mov dx, di  ; indica o endereço da porta I/O
    ;mov ax, si ; move o valor a ser enviado à porta
    ;out dx, al  ; envia o dado para a porta I/O

    mov dx, di  ; indica o endereço da porta I/O
    mov eax, esi ; move o valor a ser enviado à porta
    out dx, eax  ; envia o dado para a porta I/O
    ret

;**************************************************************************
;uint32_t __read_portd(uint16_t port);
;--------------------------------------------------------------------------
;Finalidade: É a rotina que manipula dados para as portas I/O
;--------------------------------------------------------------------------
;Parâmetros: 
;   1 - port(RDI);  ;   
;--------------------------------------------------------------------------
;Retorna: void
;--------------------------------------------------------------------------
global __read_portd
__read_portd:
    mov dx, di  ; indica o endereço da porta I/O
    in eax, dx  ; envia o dado da porta I/O para AX
    ret	

;**************************************************************************	