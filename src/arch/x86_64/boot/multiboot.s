;/*--------------------------------------------------------------------------
;*  File name:  multiboot.s
;*  Author:  Aldenor Sombra de Oliveira
;*  Data de criação: 10-05-2020
;*--------------------------------------------------------------------------
;Este arquivo é necessário para utilizar o bootloader QEMU ou BOCHS
;--------------------------------------------------------------------------*/

;/*  The magic field should contain this. */
%define MULTIBOOT2_HEADER_MAGIC     0xE85250D6

section .multiboot_header
align 8 				; Must be 8 byte aligned as per the specification
header_start:	
	dd MULTIBOOT2_HEADER_MAGIC  	; Magic number (multiboot 2)
	dd 0			     	; Architecture 0 (protected mode i386)
	dd header_end - header_start 	; Header length
	
	; Checksum
	dd 0x100000000 - (0xE85250D6 + 0 + (header_end - header_start))

	; Page align modules (simplifies initrd logic)
	dw 6
	dw 0
	dd 8

	; Required end tag
	dw 0
	dw 0
	dd 8
header_end:
