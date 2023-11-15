;/*--------------------------------------------------------------------------
;*  File name:  x86_64.s
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 17-09-2023
;*--------------------------------------------------------------------------
;Este arquivo possui as rotinas para a manipulação de interrupções 
;--------------------------------------------------------------------------*/
%ifndef _X86_64_INC
%define _X86_64_INC

%macro PUSH_ALL 0
  push rax
  push rbx
  push rcx
  push rdx
  push rsi
  push rdi
  push rbp
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15
%endmacro

%macro POP_ALL 0
  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop rbp
  pop rdi
  pop rsi
  pop rdx
  pop rcx
  pop rbx
  pop rax
%endmacro


;-----------------------------------------------------------
%macro RELOAD_CS 1
;-----------------------------------------------------------
;Macro que insere no código assembly um far jump para
;carregar o register CS após a troca da GDT.
;Uso: reload_CS "Code segment selector"
;Estrutura ".far_ptr" utilizada com "jump far indireto". Passamos o
;endereço inicial na memória e o sistema faz a atribuição CS:RIP.
;-----------------------------------			
	lea rax, [rel %%.far_ptr]			
	jmp far [rax]	
;------------------------------------
%%.far_ptr:	
	dq  %%.fix_cs	
  dw %1
;-------------------------------------
%%.fix_cs:	
;------------------------------------------------------------

%endmacro

%endif