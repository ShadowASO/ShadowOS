/*--------------------------------------------------------------------------
*  File name:  vmm.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 04-10-2020
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a alocação de
páginas físicas.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "string.h"
#include "mm/vmm.h"
#include "mm/page.h"
#include "x86_64.h"
#include "stdio.h"
#include "debug.h"
#include "../info.h"
#include "mm/pgtable.h"
#include "mm/pgtable_types.h"
#include "ktypes.h"
#include "mm/zone.h"
#include "mm/bootmem.h"

/**
 * Recebe um endereço virtual  e devolve o endereço virtual da pagetable
 * indicada em page, utilizando o método recursivo.
 * A solução adotada nesta função consiste em criar duas máscaras independentes
 * para o índice recursivo e outra para o índice do endereço original. A solu-
 * ção evita utilizar laços e testes.
 */
u64_t get_pagetable_address(virt_addr_t virtual_address, int page)
{
    u64_t vaddr = (u64_t)virtual_address;
    if (page < 1 || page > 4)
    {
        return 0;
    }

    // Ajusto os indices
    int offset1 = (INDEX_SHIFT * page);
    u64_t msk_index = (vaddr & MASK_VPN_INDEX);
    msk_index = (msk_index >> offset1) & MASK_VPN_INDEX;

    // Ajusto a máscara de recursivo
    int offset2 = (INDEX_SHIFT * (4 - page));
    u64_t msk_recur = MASK_VPN_RECUR << offset2;
    msk_recur = (msk_recur & MASK_VPN_INDEX);

    // Monto o endereço da tabela
    u64_t addr = ((u64_t)virtual_address | MASK_VPN_INDEX) ^ MASK_VPN_INDEX;
    addr = MASK_PAGE_ENTRY(addr);
    addr = (u64_t)addr | (u64_t)msk_index | (u64_t)msk_recur;

    return addr;
}

/**
 * @brief Devolve o endereço virtual de qaulquer dos pageframes que contém os
 * pageframes do endereço.
 * no endereço.
 *
 * @param virtual_address
 * @param pgt_no
 * @return u64_t
 */
u64_t get_pgtable_vaddress(virt_addr_t virtual_address, int pgt_no)
{
    u64_t vaddr = (u64_t)virtual_address;

    if (pgt_no < 1 || pgt_no > 4)
    {
        return (u64_t)NULL;
    }

    u64_t addr = (vaddr | VIRTUAL_ADDRESS_MASK) ^ VIRTUAL_ADDRESS_MASK;
    u64_t addr_entry = vaddr & VIRTUAL_ADDRESS_MASK; // apenas entries
    u64_t addr_recu = 0;

    /* Desloco os campos do endereço para acomodar o número de entradas recursivas.
    -------------------------------------
    | P4 | P3 | P2 | P1 | PAGE_OFFSET   |
    ------------------------------------- */

    addr_entry = addr_entry >> (INDEX_SHIFT * (pgt_no));
    addr_entry = MASK_PAGE_ENTRY(addr_entry);

    /*  O uso do switch case torna a rotina mais rápida do que utilizando um
         loop for. A variável offset deve ser decrementada e o valor restante vai
         ser utilizado para deslocar os campos recursivos para a esquerda */

    int offset = 4;
    switch (pgt_no)
    {
    case P4_ID:
        addr_recu = addr_recu | ENTRY_RECURSIVA;
        addr_recu = addr_recu << (INDEX_SHIFT);
        offset--;
        [[fallthrough]];
    case P3_ID:
        addr_recu = addr_recu | ENTRY_RECURSIVA;
        addr_recu = addr_recu << (INDEX_SHIFT);
        offset--;
        [[fallthrough]];

    case P2_ID:
        addr_recu = addr_recu | ENTRY_RECURSIVA;
        addr_recu = addr_recu << (INDEX_SHIFT);
        offset--;
        [[fallthrough]];

    case P1_ID:
        // Qualquer chamada, para ser válida, vai entrar neste case. Por isso,
        // faço o delocamento relativo ao offset e page offset
        offset--;
        addr_recu = addr_recu | ENTRY_RECURSIVA;
        addr_recu = addr_recu << (INDEX_SHIFT * offset + PAGE_SHIFT);
        [[fallthrough]];

    default:
        break;
    }
    return (addr | addr_entry | addr_recu);
}

/**
 * Recebe um endereço virtual  e devolve o endereço virtual da entrada(entry)
 * na pagetable indicada em page, utilizando o método recursivo.
 */
u64_t get_pagetable_entry_address(virt_addr_t virtual_address, int page)
{
    u64_t vaddr = (u64_t)virtual_address;
    if (page < 1 || page > 4)
    {
        return 0;
    }

    // Calculo o deslocamento dentro da tabela
    int offset1 = (page - 1) * INDEX_SHIFT + PAGE_SHIFT;
    u64_t tmp_entry_idx = (vaddr >> offset1) & INDEX_MASK;

    /* Ajusto os indices do endereço, deslocando para a direita de acordo com o
    número da tabela indicada */

    int offset2 = INDEX_SHIFT * page;
    u64_t msk_index = (vaddr & MASK_VPN_INDEX);
    msk_index = (msk_index >> offset2) & MASK_VPN_INDEX;
    msk_index = (u64_t)msk_index | (u64_t)tmp_entry_idx * 8;

    // Ajusto a máscara de recursividade para que ocupe os índices à direita
    // do endereço virtual.

    int offset3 = INDEX_SHIFT * (4 - page);
    u64_t msk_recur = MASK_VPN_RECUR << offset3;
    msk_recur = (msk_recur & MASK_VPN_INDEX);

    // Monto o endereço da tabela
    u64_t addr = ((u64_t)virtual_address | MASK_VPN_INDEX) ^ MASK_VPN_INDEX;
    addr = MASK_PAGE_ENTRY(addr);
    addr = (u64_t)addr | (u64_t)msk_index | (u64_t)msk_recur;

    return addr;
}

/**
 * @brief Recebe o endereço virtual de um pagetable e o index do entry
 * desejado.
 *
 * @param vtable_address
 * @param entry_index
 * @return GenericEntry_t
 */

static inline page_entry_t table_entry(u64_t vtable_address, int entry_index)
{
    return ((page_entry_t *)vtable_address)[entry_index];
}

/**
 * Lista os registros entry da pagetable indicada pelo endereço virtual
 */
void dump_page_entry(virt_addr_t vtable_address)
{

    for (int i = 0; i < 512; i++)
    {
        page_entry_t entry = table_entry((u64_t)vtable_address, i);

        if (entry.pte.flags.P)
        {
            kprintf("\n  %d - %x", i, entry.value);
        }
    }
}
static inline phys_addr_t map_p3(p4e_t *p4e, u64_t pg_flags)
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
        pgframe = alloc_pagetable();
        clear_frame(phys_to_virt(pgframe));
        (*p4e).p4e = pgframe | pg_flags;

        /* PageTable statistic. */
        mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, P3_ID);
    }
    return (pgframe);
}
static inline phys_addr_t map_p2(p3e_t *p3e, u64_t pg_flags)
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
        pgframe = alloc_pagetable();
        clear_frame(phys_to_virt(pgframe));
        (*p3e).p3e = pgframe | pg_flags;

        /* PageTable statistic. */
        mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, P2_ID);
    }
    return (pgframe);
}
static inline phys_addr_t map_p1(p2e_t *p2e, u64_t pg_flags)
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
        pgframe = alloc_pagetable();
        clear_frame(phys_to_virt(pgframe));
        (*p2e).p2e = pgframe | pg_flags;

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
static inline phys_addr_t map_vmm_frame(p1e_t *p1e, phys_addr_t phys_addr, u64_t pg_flags)
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

virt_addr_t kmap_frame(virt_addr_t v_addr, phys_addr_t phys_addr, u64_t pgtable_flags)
{
    // Verifico o alinhamento dos endereços em 4096 bytes
    if (!is_aligned((u64_t)v_addr, PAGE_SIZE))
    {
        kprintf("\nERRO: endereço virtual desalinhado: %x - %x", v_addr, phys_addr);
        pause_enter();
        return NULL;
    }
    else if (!is_aligned(phys_addr, PAGE_SIZE))
    {
        kprintf("\nERRO: endereço fisico desalinhado: %x - %x", v_addr, phys_addr);
        return NULL;
    }

    //************************************ DEBUG
    if (phys_addr == 0xFEE00000 || phys_addr == 0xFEC00000)
    {
        kprintf("\n(*) virtual_addr=%p - phys_addr=%x - flags_tabs=%x",
                v_addr, phys_addr, pgtable_flags);
    }

    // debug_pause("p1");

    p1e_t *p1e = kmap_tables_core(&init_mm, v_addr, pgtable_flags);
    map_vmm_frame(p1e, phys_addr, pgtable_flags);

    // Invalido e atualizo a tlb
    __vm_invlpg_tlb((u64_t)v_addr);

    return v_addr;
}

phys_addr_t kunmap_frame(virt_addr_t v_addr)
{
    phys_addr_t pgframe = 0;
    virt_addr_t pml4 = init_mm.pml4;

    //*****************************
    // p4e_t *p4e = p4_entry(&init_mm, (u64_t)v_addr);
    p4e_t *p4e = p4_entry(pml4, (u64_t)v_addr);
    if (!p3_present(p4e))
    {
        return pgframe;
    }

    // Mapeia um pagetable2 para a entrada no P3
    p3e_t *p3e = p3_entry(p4e, (u64_t)v_addr);
    if (!p2_present(p3e))
    {
        return pgframe;
    }

    // Mapeia um pagetable1 para a entrada no P2
    p2e_t *p2e = p2_entry(p3e, (u64_t)v_addr);
    if (!p1_present(p2e))
    {
        return pgframe;
    }

    // Mapeio o pageframe
    p1e_t *p1e = p1_entry(p2e, (u64_t)v_addr);
    pgframe = p1e->p1e;
    p1e->p1e = 0;

    __vm_invlpg_tlb((u64_t)v_addr);
    return pgframe;
}

/**
 * @brief Faz o mapeamento do endereço virtual informado, utilizando o pageframe
 * indicado pelo endereço físico. Caso haja a necessidade de criar pagetables, a
 * própria rotina o faz. O endereço virtual e o endereço físico devem estar ali-
 * nhados em 4096 bytes. Se não estiver, exibe mensagem de erro e devolve um NULL.
 * O usuário deve testar o valor de retorno.
 * @param mm
 * @param v_addr
 * @param phys_addr
 * @param pgtable_flags
 * @return
 */
virt_addr_t kmap_tables_core(mm_struct_t *mm, virt_addr_t v_addr, u64_t pgtable_flags)
{
    virt_addr_t pml4 = init_mm.pml4;

    //*****************************
    // p4e_t *p4e = p4_entry(mm, (u64_t)v_addr);
    p4e_t *p4e = p4_entry(pml4, (u64_t)v_addr);
    map_p3(p4e, pgtable_flags);

    // Mapeia um pagetable2 para a entrada no P3
    p3e_t *p3e = p3_entry(p4e, (u64_t)v_addr);
    map_p2(p3e, pgtable_flags);

    // Mapeia um pagetable1 para a entrada no P2
    p2e_t *p2e = p2_entry(p3e, (u64_t)v_addr);
    map_p1(p2e, pgtable_flags);

    // Mapeio a entrada do PageTable.
    p1e_t *p1e = p1_entry(p2e, (u64_t)v_addr);

    // kprintf("\np4e=%p - p3e=%p - p2e=%p - p1e=%p", p4e, p3e, p2e, p1e);
    //  debug_pause("p1");
    return p1e;
}
