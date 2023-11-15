/*--------------------------------------------------------------------------
*  File name:  interrupt.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 21-11-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a tabela
IDT e seus descritores, necessários para a manipulação das interrupções.
--------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "stdlib.h"
#include "ktypes.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "exception.h"
#include "interrupt.h"
#include "percpu.h"
#include "tss.h"
#include "gdt.h"
#include "idt.h"
#include "lapic.h"

void interrupt_ini(void)
{
    setup_pic();

    setup_isr();

    exception_init();
}