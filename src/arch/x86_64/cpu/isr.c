/*--------------------------------------------------------------------------
*  File name:  isr.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 01-08-2020
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a tabela
IDT e seus descritores, necessários para a manipulação das interrupções.
--------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "stdlib.h"
#include "string.h"
#include "idt.h"
#include "x86_64.h"
#include "screen.h"
#include "isr.h"
#include "stdio.h"
#include "keyboard.h"
#include "debug.h"
#include "lapic.h"
#include "task.h"
#include "interrupt.h"
#include "percpu.h"
#include "scheduler.h"
#include "tss.h"

void isr_task_handler(cpu_regs_t *tsk_contxt);
void isr_global_handler(cpu_regs_t *tsk_contxt);

extern task_t *task_ini;

/*------------------------------------------------------------------------------*/
/* Esta tabela serve para configurar uma rotina a ser executada para cada interrupção.
É muito útil para permitir uma configuração personalizada para cada interrupção.*/
isr_obj_t isr_handlers[256] = {0};
/*------------------------------------------------------------------------------*/

/* Limpa a tabela isr_handlers.*/
void setup_isr(void)
{
	memset(isr_handlers, 0, sizeof isr_handlers);
}
/* Carrega a tabela "isr_handlers" com um handler/rotina a ser executada quando
ocorrer uma interrupção com vetor "vec". */
static inline void add_isr_handler(uint8_t vec, enum isr_type type, isr_handler_t handler)
{
	isr_handlers[vec].type = type;
	isr_handlers[vec].handler = handler;
}
/*Rotinas a serem utilizadas para a atribuição de um handler, segundo o tipo:
ISR_HANDLER_IRQ, ISR_HANDLER_EXCEPTION, ISR_HANDLER_IPI, ISR_HANDLER_NOP. */
void add_handler_irq(uint8_t vec, isr_handler_t handler)
{
	add_isr_handler(vec, ISR_HANDLER_IRQ, handler);
}
void add_handler_exception(uint8_t vec, isr_handler_t handler)
{
	add_isr_handler(vec, ISR_HANDLER_EXCEPTION, handler);
}
void add_handler_ipi(uint8_t vec, isr_handler_t handler)
{
	add_isr_handler(vec, ISR_HANDLER_IPI, handler);
}
void add_handler_nop(uint8_t vec, isr_handler_t handler)
{
	add_isr_handler(vec, ISR_HANDLER_NOP, handler);
}

/* Todas as interrupções iniciam neste handler e são distribuídas
 * conforme sejam intrrupções ou exceções. */

void isr_global_handler(cpu_regs_t *tsk_contxt)
{
	uint32_t vec_no = tsk_contxt->int_no & 0xFF;
	isr_obj_t *isr_obj = &isr_handlers[vec_no];
	static u8_t y = 0;

	/* Debugar a stack.                      */
	_stack_rbp = (mm_addr_t)&tsk_contxt->old_ss;
	_stack_rsp = _stack_rbp;
	/*--------------------------------------*/

	if (isr_obj->type == ISR_HANDLER_IRQ)
	{
		if (vec_no == ISR_VECTOR_KEYBOARD && false)
		{

			kprintf("\nCPU=%d", cpu_id());
		}

		//   WARN_ON("CPU=%d", cpu_id());
		apic_eoi();
	}
	if (isr_obj->handler != NULL)
	{
		/* Se houve uma rotina inserida no "isr_handlers" para esse vetor,
		ela é executada aqui. */
		isr_obj->handler(tsk_contxt);
	}
	/* Verifica se percpu->preempt_count==0(preempt enabled)
	if (vec_no == ISR_VECTOR_TIMER && percpu_reschedule())
	{
		// WARN_ON("&tsk_contxt->ss=%p -> stack->rsp=%p --> ori->rsp=%p ", &tsk_contxt->ss, tsk_contxt, tsk_contxt->rsp);
		scheduler();
	}
	*/

	if (vec_no == ISR_VECTOR_SPURIOUS)
	{
		kprintf("\nIRQ - Apic Spuriour=%d", vec_no);
	}
	return;
}
