/*--------------------------------------------------------------------------
*  File name:  vm_area.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 21-07-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui rotinas dirigidas ao pameamento virtual de áreas
de memória.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "mm/vm_area.h"
#include "mm/pgtable_types.h"
#include "mm/pgtable.h"
#include "mm/vmm.h"
#include "debug.h"
#include "mm/tlb.h"
#include "mm/kmalloc.h"

static inline phys_addr_t alloc_pagetble_frame(vm_flags_t flags)
{

    return alloc_frames(GFP_ZONE_NORMAL, 0);
}

static inline phys_addr_t kalloc_p3(p4e_t *p4e, pgprot_t pg_prot, vm_flags_t flags)
{
    phys_addr_t pgframe = 0;

    // Se o pagetable já está mapeado, devolve o seu endereço físico.
    if (p3_present(p4e))
    {
        pgframe = MASK_PAGE_ENTRY((*p4e).p4e);
    }
    // Aloca um novo pageframe e insere seu endereço físico no entry
    else
    {
        pgframe = alloc_pagetble_frame(flags);
        clear_frame(phys_to_virt(pgframe));
        (*p4e).p4e = pgframe | (pg_prot.value);

        /* PageTable statistic. */
        mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, P3_ID);
    }
    return (pgframe);
}
static inline phys_addr_t kalloc_p2(p3e_t *p3e, pgprot_t pg_prot, vm_flags_t flags)
{
    phys_addr_t pgframe = 0;

    // Se o pagetable já está mapeado, devolve o seu endereço físico.
    if (p2_present(p3e))
    {
        pgframe = MASK_PAGE_ENTRY((*p3e).p3e);
    }
    // Aloca um novo pageframe e insere seu endereço físico no entry
    else
    {
        pgframe = alloc_pagetble_frame(flags);

        clear_frame(phys_to_virt(pgframe));
        (*p3e).p3e = pgframe | (pg_prot.value);

        /* PageTable statistic. */
        mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, P2_ID);
    }
    return (pgframe);
}
static inline phys_addr_t kalloc_p1(p2e_t *p2e, pgprot_t pg_prot, vm_flags_t flags)
{
    phys_addr_t pgframe = 0;

    // Se o pagetable já está mapeado, devolve o seu endereço físico.
    if (p1_present(p2e))
    {
        pgframe = MASK_PAGE_ENTRY((*p2e).p2e);
    }
    // Aloca um novo pageframe e insere seu endereço físico no entry
    else
    {

        pgframe = alloc_pagetble_frame(flags);
        clear_frame(phys_to_virt(pgframe));
        (*p2e).p2e = pgframe | (pg_prot.value);

        /* PageTable statistic. */
        mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, P1_ID);
    }
    return (pgframe);
}

/**
 * @brief Rotina genérica que recebe o endereço de um entry em P1 e o endereço físico
 * de um pageframe e faz o seu mapeamento.
 *
 * @param p1e
 * @param phys_addr
 * @param pg_flags
 * @return phys_addr_t
 */
static inline phys_addr_t map_area_frame(p1e_t *p1e, phys_addr_t phys_addr, u64_t pg_flags)
{
    phys_addr_t pgframe = 0;

    // Se o p1e já está mapeando um pageframe, devolve o seu endereço físico.
    // Mas continuo o mapeamento do novo frame informado em phys_addr
    if ((p1e->p1e) & PG_FLAG_P)
    {
        pgframe = MASK_PAGE_ENTRY((*p1e).p1e);
    }
    // Executo o bloco seguinte, inserindo o novo frame
    else
    {
        p1_entry_t pte;
        pte.value = phys_addr | pg_flags;
        (p1e)->p1e = pte.value;
        pgframe = MASK_PAGE_ENTRY((*p1e).p1e);

        /* PageTable statistic. */
        mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, PFRAME_ID);
    }

    return (pgframe);
}
/**
 * @brief Rotina faz o mapeamento dos pagetables e devolve uma
 * entry no tablePage1 no qual deve ser inserido o dendereço
 * físico de um frame. Este esquema dá mais flexibilidade para
 * o mapeamento.
 *
 * @param vmm
 * @param vaddr
 * @return p1e_t*
 */
static p1e_t *map_pagetables(vm_area_t *vmm, virt_addr_t vaddr)
{

    virt_addr_t pml4 = init_mm.pml4;
    //*****************************

    p4e_t *p4e = p4_entry(pml4, (u64_t)vaddr);
    kalloc_p3(p4e, vmm->pgprot, vmm->flags);

    //  Mapeia um pagetable2 para a entrada no P3
    p3e_t *p3e = p3_entry(p4e, (u64_t)vaddr);
    kalloc_p2(p3e, vmm->pgprot, vmm->flags);

    // Mapeia um pagetable1 para a entrada no P2
    p2e_t *p2e = p2_entry(p3e, (u64_t)vaddr);
    kalloc_p1(p2e, vmm->pgprot, vmm->flags);

    // Mapeio o pageframe
    p1e_t *p1e = p1_entry(p2e, (u64_t)vaddr);

    return p1e;
}
static void mmap_mem_range(vm_area_t *vmm)
{
    mm_addr_t ini_range = (mm_addr_t)vmm->vm_start;
    mm_addr_t pend_range = (mm_addr_t)vmm->vm_pend;
    u8_t order = calc_bin_order((pend_range - ini_range) / PAGE_SIZE);

    phys_addr_t pageframe = 0;
    u64_t prot_tmp = vmm->pgprot.value;

    kprintf("\nini_range=%p", ini_range);
    kprintf("\npend_range=%p", pend_range);
    kprintf("\nprot_tmp=%x - order=%d", prot_tmp, order);

    if (vmm->flags & VM_IDENTI)
    {
        pageframe = ini_range;
    }
    else if (vmm->flags & VM_RANGER)
    {
        pageframe = vmm->phys_base;
    }
    else if (vmm->flags & VM_HEAP)
    {
        pageframe = alloc_heap_block(order);
        kprintf("\npageframe=%x", pageframe);
    }

    else
    {
        pageframe = alloc_pagetable();
    }

    // for (; ini_range < pend_range; ini_range += PAGE_SIZE)
    do
    {

        /*
        Esta rotina é executada para todos os frames do intervalo
        de memória.
        */
        p1e_t *p1e = map_pagetables(vmm, (virt_addr_t)ini_range);

        if ((p1e->p1e) & PG_FLAG_P)
        {
            kprintf("\np1e=%x ja existe", p1e->p1e);
            pause_enter();
            continue;
        }

        // pageframe = kalloc_frames(GFP_ZONE_NORMAL_ID, 0);
        // pageframe = alloc_pagetable();

        if (!pageframe)
        {
            kprintf("\nPageframe == %x. Memory esgotada.", pageframe);
            pause_enter();
        }
        /**
         * @brief Faz o mapeamento do frame, que tem o seu endereço
         * físico inserido no entry do PageTable1.
         */

        map_area_frame(p1e, pageframe, prot_tmp);

        ini_range += PAGE_SIZE;

        if ((ini_range < pend_range))
        {
            if ((vmm->flags & VM_IDENTI) || (vmm->flags & VM_RANGER) || (vmm->flags & VM_HEAP))
            {
                pageframe = pageframe + PAGE_SIZE;
            }
            else
            {
                pageframe = alloc_pagetable();
            }
        }

    } while (ini_range < pend_range);

    kprintf("\npageframe=%x", pageframe);
    // debug_pause("p1");
}

void kmmap(vm_area_t *vmm_area)
{
    kprintf("\n%s(%d) - Mapeando area virtual.", __FUNCTION__, __LINE__);

    virt_addr_t pml4 = init_mm.pml4;

    if (pml4 == NULL)
    {
        kprintf("\nLinha: %d - ERROR - VMM_AREA: init_mm.pml4==NULL.", __LINE__);
        debug_pause("\nErro fatal.");
    }

    mmap_mem_range(vmm_area);
    flush_tlb_range((phys_addr_t)vmm_area->vm_start, (phys_addr_t)vmm_area->vm_pend);

    kprintf("\n%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}
