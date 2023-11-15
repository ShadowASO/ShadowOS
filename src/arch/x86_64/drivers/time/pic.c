/*--------------------------------------------------------------------------
*  File name:  pic.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 03-07-2021
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a tabela
IDT e seus descritores, necessários para a manipulação das interrupções.
--------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "string.h"
#include "io.h"
#include "isr.h"
#include "pic.h"
#include "x86_64.h"

void setup_pic(void)
{
    //__disable_irq();

    /* ICW1 - begin initialization */
    __write_portb(PIC1_COMMAND, 0x11);
    __write_portb(PIC2_COMMAND, 0x11);

    /* ICW2 - remap offset address of idt_table */
    /*
     * In x86 protected mode, we have to remap the PICs beyond 0x20 because
     * Intel have designated the first 32 interrupts as "reserved" for cpu exceptions
     */
    __write_portb(PIC1_DATA, 0x20); // Make PIC1 start at 32
    __write_portb(PIC2_DATA, 0x28); // Make PIC2 start at 40

    // Tell master there is slave
    __write_portb(PIC1_DATA, 0x04);
    // Tell slave cascade identity
    __write_portb(PIC2_DATA, 0x02);

    //************************************
    // ICW3 - setup cascading
    __write_portb(PIC1_DATA, 0x00);
    __write_portb(PIC2_DATA, 0x00);
    //********************************

    /* ICW4 - environment info */
    __write_portb(PIC1_DATA, 0x01);
    __write_portb(PIC2_DATA, 0x01);

    // Desabilita o PIC
    __write_portb(PIC1_DATA, 0xFF);
    __write_portb(PIC2_DATA, 0xFF);

    // memset(isr_handlers, 0, sizeof isr_handlers);
    //__enable_irq();
}

/**
 * Esta rotina mascara ou desmascara uma linha no PIC
 */
void pic_mask_irq(uint8_t Line, int32_t Set)
{
    uint16_t Port;

    if (Line < 8)
        Port = 0x21;
    else
    {
        Port = 0xA1;
        Line -= 8;
    }

    unsigned char Val;
    if (Set)
        Val = __read_portb(Port) | (1 << Line);
    else
        Val = __read_portb(Port) & ~(1 << Line);

    __write_portb(Port, Val);
}

/**
 * Ao final de toda interrupção, devemos enviar ao PIC o comando de fim de interrupção (EOI)
 */
void pic_eoi(uint32_t irq)
{
    if (irq >= 40)
        __write_portb(PIC2, PIC_EOI);

    __write_portb(PIC1, PIC_EOI);
}

void pic_disable(void)
{
    // FUNC_ENTER();
    __write_portb(0x20, PIC1_COMMAND);
    __write_portb(0x20, PIC2_COMMAND);
    __write_portb(0xff, PIC1_DATA);
    __write_portb(0xff, PIC2_DATA);
}