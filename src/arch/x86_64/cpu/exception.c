/*--------------------------------------------------------------------------
*  File name:  exception.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 20-11-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a tabela
IDT e seus descritores, necessários para a manipulação das interrupções.
--------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "stdlib.h"
#include "x86_64.h"
#include "isr.h"
#include "exception.h"
#include "interrupt.h"
#include "debug.h"
#include "bits.h"
#include "scheduler.h"

static const char *const exception_messages[32] = {
    "Division by zero",
    "Debug",
    "NMI - Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "(reserved exception 9)",
    "Invalid TSS",
    "Segment not present",
    "Stack segment fault",
    "General protection fault",
    "Page fault",
    "(reserved exception 15)",
    "x87 floating-point exception",
    "Alignment check",
    "Machine check",
    "SIMD floating-point exception",
    "Virtualization exception",
    "(reserved exception 21)",
    "(reserved exception 22)",
    "(reserved exception 23)",
    "(reserved exception 24)",
    "(reserved exception 25)",
    "(reserved exception 26)",
    "(reserved exception 27)",
    "(reserved exception 28)",
    "(reserved exception 29)",
    "(reserved exception 30)",
    "(reserved exception 31)"};

static void page_fault_handler(cpu_regs_t *tsk_contxt);
static void exception_handler(cpu_regs_t *tsk_contxt);

void exception_init(void)
{
    for (uint8_t i = 0; i < 32; i++)
    {
        if (i == EXCEPTION_PAGE_FAULT)
        {
            add_handler_exception(i, page_fault_handler);
        }
        else
        {
            add_handler_exception(i, exception_handler);
        }
    }
}
static void exception_handler(cpu_regs_t *tsk_contxt)
{
    uint32_t intNum = tsk_contxt->int_no & 0xFF;

    switch (intNum)
    {
    case EXCEPTION_GENERAL_PROTECTION:

        WARN_ON("%d - %s", intNum, exception_messages[intNum]);
        dump_contexto(tsk_contxt);
        while (true)
            ;

        break;

    default:
        kprintf(
            "%s:\n"
            "\trip: %p, rsp: %p\n"
            "\tint_no: %u, err_code: %lu",
            exception_messages[intNum],
            (void *)tsk_contxt->rip, (void *)tsk_contxt->old_rsp,
            intNum, (tsk_contxt->err_code & 0xFFFFFFFF));
    }

    dump_contexto(tsk_contxt);
    DBG_STOP("Ocorreu uma exception.");
}
static void page_fault_handler(cpu_regs_t *tsk_contxt)
{

    u64_t faulting_address = __read_cr2();
    u64_t erro_code = tsk_contxt->err_code;

    // The error code gives us details of what happened.
    int present = !(tsk_contxt->err_code & 0x1); // Page not present
    int rw = tsk_contxt->err_code & 0x2;         // Write operation?
    int us = tsk_contxt->err_code & 0x4;         // Processor was in user-mode?
    int reserved = tsk_contxt->err_code & 0x8;   // Overwritten CPU-reserved bits of page entry?
    int id = tsk_contxt->err_code & 0x10;        // Caused by an instruction fetch?

    /*
    // The fault was likely due to an access in kernel space, so give up
    if (faulting_address & (1ULL << 63))
        goto kernel_panic;

    // If interrupts were enabled, we are safe to enable them again
    if (regs->rflags & 0x200)
        sti();

    if (current != NULL) {
        virt_addr_t aligned_addr = (virt_addr_t)(faulting_address & ~(PAGE_SIZE - 1));

        write_spin_lock(&current->mmu->pgtab_lock);

        pte_t *pte = vmm_get_pte(current->mmu, aligned_addr);
        bool done = regs->info & PAGE_FAULT_RW && cow_handle_write(pte, aligned_addr);

        write_spin_unlock(&current->mmu->pgtab_lock);

        if (done)
            return;
    }
*/
    // TODO: Kill process
    // Output an error message.
    kprintf("\n\nERRO: %d - ", tsk_contxt->int_no);
    kprintf("Page fault! ");
    kprintf("- Address: ");
    kprintf("[ %p ] ", faulting_address);
    if (erro_code & BIT(0))
    {
        kprintf("\n. Page-level protection violation. ");
    }
    else
    {
        kprintf("\n. Page not-present. ");
    }
    /* Read or Write. */
    if (erro_code & BIT(1))
    {
        kprintf("\n. On WRITE. ");
    }
    else
    {
        kprintf("\n. On READ. ");
    }
    /* Mode de operação: User or Kernel. */
    if (erro_code & BIT(2))
    {
        kprintf("\n. User Mode. ");
    }
    else
    {
        kprintf("\n. Kernel Mode. ");
    }
    /* fetch instruction or data. */
    if (erro_code & BIT(4))
    {
        kprintf("\n. Fault on instruction fetch. ");
    }
    else
    {
        kprintf("\n. Fault on DATA fetch. ");
    }

    kprintf("\n");
    DBG_PAUSE("Pause");
}

void dump_contexto(cpu_regs_t *task_context)
{
    // u64_t *tss = __get_curr_tss();
    u64_t *stack = 0;

    // kprintf("\nTSS_IST3=%x", __tss_ist_read(tss, TSS_IST3));
    kprintf("\n\ntask_context->ss=%x", task_context->old_ss);
    kprintf("\ntask_context->rsp=%x", task_context->old_rsp);
    kprintf("\ntask_context->rflags=%x", task_context->rflags);
    kprintf("\ntask_context->cs=%x", task_context->cs);
    kprintf("\ntask_context->rip=%x", task_context->rip);

    kprintf("\ntask_context->rax=%x", task_context->rax);
    kprintf("\ntask_context->rbx=%x", task_context->rbx);
    kprintf("\ntask_context->rcx=%x", task_context->rcx);
    kprintf("\ntask_context->rdx=%x", task_context->rdx);
    kprintf("\ntask_context->rsi=%x", task_context->rsi);
    kprintf("\ntask_context->rdi=%x", task_context->rdi);
    kprintf("\ntask_context->rbp=%x", task_context->rbp);
    kprintf("\ntask_context->r8=%x", task_context->r8);
    kprintf("\ntask_context->r9=%x", task_context->r9);
    kprintf("\ntask_context->r10=%x", task_context->r10);
    kprintf("\ntask_context->r11=%x", task_context->r11);
    kprintf("\ntask_context->r12=%x", task_context->r12);
    kprintf("\ntask_context->r13=%x", task_context->r13);
    kprintf("\ntask_context->r14=%x", task_context->r14);
    kprintf("\ntask_context->r15=%x", task_context->r15);
}