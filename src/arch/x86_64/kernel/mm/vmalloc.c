/*--------------------------------------------------------------------------
*  File name:  vmalloc.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 01-07-2023
*--------------------------------------------------------------------------
Este arquivo fonte possui rotinas dirigidas ao pameamento virtual de áreas
de memória.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"
#include "mm/mm_types.h"
#include "mm/vm_area.h"
#include "mm/pgtable_types.h"
#include "mm/pgtable.h"
#include "mm/vmm.h"
#include "debug.h"
#include "mm/tlb.h"
#include "mm/kmalloc.h"
#include "mm/vmalloc.h"

vmalloc_area_t vmalloc_areas;

/**
 * Calcular o level com tamanho adequado para comportar a memória
 * requisitada, adicionando o tamanho do header
 */
static inline u8_t calc_list_order(size_t mm_size)
{
    size_t i = calc_bin_order(mm_size >> PAGE_SHIFT);

    return i;
}

/**
 * kzalloc_node - allocate zeroed memory from a particular memory node.
 * @size: how many bytes of memory are required.
 * @flags: the type of memory to allocate (see kmalloc).
 * @node: memory node from which to allocate
 */
static inline void *kzalloc(size_t size, gfp_t flags)
{
    virt_addr_t zbck = kmalloc(size);
    if (unlikely(!zbck))
        return NULL;

    memset(zbck, 0, size);
    return zbck;
}

static inline phys_addr_t vmap_p3(p4e_t *p4e, pgprot_t pg_prot)
{
    phys_addr_t frame = 0;

    /* Se o pagetable já está mapeado, devolve o seu endereço físico.
    Caso contrário, aloca um novo pageframe. */
    if (p3_present(p4e))
    {
        frame = MASK_PAGE_ENTRY((*p4e).p4e);
    }
    else
    {
        frame = alloc_frame(GFP_ZONE_NORMAL);
        clear_frame(phys_to_virt(frame));
        (*p4e).p4e = frame | pg_prot.value;

        /* PageTable statistic. */
        mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, P3_ID);
    }
    return (frame);
}
static inline phys_addr_t vmap_p2(p3e_t *p3e, pgprot_t pg_prot)
{
    phys_addr_t frame = 0;

    /* Se o pagetable já está mapeado, devolve o seu endereço físico.
    Caso contrário, aloca um novo pageframe. */
    if (p2_present(p3e))
    {
        frame = MASK_PAGE_ENTRY((*p3e).p3e);
    }
    else
    {
        frame = alloc_frame(GFP_ZONE_NORMAL);
        clear_frame(phys_to_virt(frame));
        (*p3e).p3e = frame | pg_prot.value;

        /* PageTable statistic. */
        mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, P2_ID);
    }
    return (frame);
}
static inline phys_addr_t vmap_p1(p2e_t *p2e, pgprot_t pg_prot)
{
    phys_addr_t frame = 0;

    /* Se o pagetable já está mapeado, devolve o seu endereço físico.
    Caso contrário, aloca um novo pageframe. */
    if (p1_present(p2e))
    {
        frame = MASK_PAGE_ENTRY((*p2e).p2e);
    }
    else
    {
        frame = alloc_frame(GFP_ZONE_NORMAL);
        clear_frame(phys_to_virt(frame));
        (*p2e).p2e = frame | (pg_prot.value);

        /* PageTable statistic. */
        mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, P1_ID);
    }
    return (frame);
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
static inline phys_addr_t map_vm_pageframe(p1e_t *p1e, phys_addr_t phys_addr, pgprot_t prot)
{
    phys_addr_t pgframe = 0;

    /* Se o p1e já está mapeando um pageframe, devolve o seu endereço físico.
    Mas continuo o mapeamento do novo frame informado em phys_addr.*/
    // if ((p1e->p1e) & PG_FLAG_P)
    if (pgframe_present(p1e))
    {
        pgframe = MASK_PAGE_ENTRY((*p1e).p1e);
    }
    else
    {
        (p1e)->p1e = phys_addr | prot.value;
        pgframe = MASK_PAGE_ENTRY((*p1e).p1e);

        /* PageTable statistic. */
        mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, PFRAME_ID);
    }

    return (pgframe);
}

/**
 * @brief A partir do endereço virtual informado, a rotina faz o mapeamento
 * dos pagetables necessários e devolve o entry no tablePage1 e no qual deve
 * ser inserido o endereço físico de um frame pela rotina que chamou.
 * Este esquema simplifica e dá mais flexibilidade ao mapeamento.
 *
 * @param vmm
 * @param vaddr
 * @return o endereço virtual de um entry em p1e_t*
 */
static p1e_t *map_vm_pagetables(mm_addr_t mm_addr, pgprot_t prot)
{

    p4e_t *p4e = pgd_offset_k(mm_addr);
    vmap_p3(p4e, prot);

    //  Mapeia um pagetable2 para a entrada no P3
    p3e_t *p3e = p3_entry(p4e, mm_addr);
    vmap_p2(p3e, prot);

    // Mapeia um pagetable1 para a entrada no P2
    p2e_t *p2e = p2_entry(p3e, mm_addr);
    vmap_p1(p2e, prot);

    // Entry na tabela P1
    p1e_t *p1e = p1_entry(p2e, mm_addr);

    return p1e;
}

static void __map_vm_range(mm_addr_t mm_addr, size_t size,
                           pgprot_t prot, struct page **pages)
{
    mm_addr_t start = mm_addr;
    mm_addr_t end = mm_addr + size;
    mm_addr_t next = start;
    size_t nr_frames = (size) >> PAGE_SHIFT;
    phys_addr_t frame = 0;

    for (size_t i = 0; i < nr_frames; i++)
    {

        /* O range é mapeado nos pagetables um page/frame por vez e devolve-se
        o endereço do entry em P1. Se o endereço já está mapeado nos pagetables,
        apenas devolve o entry em P1. */

        p1e_t *p1e = map_vm_pagetables(next, prot);

        /* Se o entre em P1 já está vinculado a um pageframe, significa que estamos
        mapeando uma área mapeada anteriormente. Possivelmente Há um erro. */
        if ((p1e->p1e) & PG_FLAG_P)
        {
            WARN_ERROR("P1 entry=%p ja está mapeado para %x", p1e, p1e->p1e);
            continue;
        }

        /* Os frames já foram alocados anteriormente e inseridos no vetor pages. */
        frame = page_to_phys(pages[i]);

        /* Se o pageframe não existe, houve um erro na allocação. */
        if (!frame)
        {
            WARN_ON("\nERROR: frame == %x. Memory esgotada.", frame);
            WARN_ERROR("ERROR: frame not alocado.Memory pode estar esgotada.");
        }

        /* Mapeia o P1 entry para o frame alocado.*/
        map_vm_pageframe(p1e, frame, prot);

        /* Avançamos para o page/frame seguinte no range. */
        next += PAGE_SIZE;
    }
}

static void map_vm_range(mm_addr_t addr, size_t size, pgprot_t prot, struct page **pages)
{
    __map_vm_range(addr, size, prot, pages);
    flush_tlb_range(addr, addr + size);
}
/* Somente limpa os entry no pagetable P1, mas não faz a exclusão dos pagetables. */
static phys_addr_t unmap_frame(mm_addr_t addr)
{
    phys_addr_t pgframe = 0;

    p4e_t *p4e = pgd_offset_k(addr);
    if (!p3_present(p4e))
    {
        return pgframe;
    }

    p3e_t *p3e = p3_entry(p4e, addr);
    if (!p2_present(p3e))
    {
        return pgframe;
    }

    p2e_t *p2e = p2_entry(p3e, addr);
    if (!p1_present(p2e))
    {
        return pgframe;
    }

    p1e_t *p1e = p1_entry(p2e, addr);
    pgframe = p1e->p1e;
    p1e->p1e = 0;

    /* PageTable statistic. */
    mm_tables_alloc_counter_dec(&init_mm.mm_pgtables, PFRAME_ID);
    // WARN_ON("\naddr=%p", addr);

    __vm_invlpg_tlb(addr);
    return pgframe;
}

static void unmap_vm_area(struct vm_struct *area)
{
    size_t size = get_vm_area_size(area);
    mm_addr_t addr = (mm_addr_t)area->addr;
    mm_addr_t end = (addr + size);
    mm_addr_t next = addr;
    phys_addr_t frame;

    for (; next < end; next += PAGE_SIZE)
    {
        frame = unmap_frame(next);
    }
    flush_tlb_range((mm_addr_t)addr, end);
}

/**
 *	remove_vm_area  -  find and remove a contingous kernel virtual area
 *
 *	@addr:		base address
 *
 *	Search for the kernel VM area starting at @addr, and remove it.
 *	This function returns the found VM area, but using it is NOT safe
 *	on SMP machines.
 */
static struct vm_struct *remove_vm_area(void *addr)
{
    struct vm_struct **p, *tmp;

    spinlock_lock(&vmalloc_areas.vmlist_lock);
    for (p = &vmalloc_areas.vmlist; (tmp = *p) != NULL; p = &tmp->next)
    {
        if (tmp->addr == addr)
            goto found;
    }
    spinlock_unlock(&vmalloc_areas.vmlist_lock);
    return NULL;

found:
    unmap_vm_area(tmp);
    *p = tmp->next;

    spinlock_unlock(&vmalloc_areas.vmlist_lock);
    return tmp;
}

static void __vunmap(void *addr, int deallocate_pages)
{
    struct vm_struct *area;

    if (!addr)
        return;

    if ((PAGE_SIZE - 1) & (unsigned long)addr)
    {
        WARN_ERROR("Trying to vfree() bad address (%p)\n", addr);
        return;
    }

    area = remove_vm_area(addr);
    if (unlikely(!area))
    {
        WARN_ERROR("Trying to vfree() nonexistent vm area (%p)\n", addr);
        return;
    }

    if (deallocate_pages)
    {
        size_t nr_pages = area->nr_pages;

        for (size_t i = 0; i < nr_pages; i++)
        {
            kprintf("\narea->pages=%p - order=%d", area->pages[i], area->pages[i]->level);
            if (unlikely(!area->pages[i]))
                WARN_ERROR("vunmap: page not allocated.");

            free_pages(area->pages[i]);
        }

        if (area->nr_pages > PAGE_SIZE / sizeof(struct page *))
            vfree(area->pages);
        else
            kfree(area->pages);
    }

    kfree(area);
    return;
}

void vmalloc_init(void)
{
    for (u8_t i = 0; i <= MAX_KMALLOC_ORDER; i++)
    {
        // INIT_LIST_HEAD(&vmalloc_areas.busy_areas[i].list);
        init_list_head(&vmalloc_areas.busy_areas[i].list);
    }

    vmalloc_areas.vm_start = get_virt_vmalloc_start();
    vmalloc_areas.vm_pend = get_virt_vmalloc_pend();
    vmalloc_areas.used_size = 0;
    vmalloc_areas.init = true;
    vmalloc_areas.vmlist = NULL;
    spinlock_init(&vmalloc_areas.vmlist_lock);
}

static void *alloc_vm_pages(struct vm_struct *area, gfp_t gfp_mask,
                            pgprot_t prot)
{
    // size_t nr_pages = get_vm_area_size(area) >> PAGE_SHIFT; // Desconta um frame de guarda
    size_t real_size = get_vm_area_size(area); /* Desconta o frame de guarda e informa somente o espaço útil. */
    size_t nr_pages = real_size >> PAGE_SHIFT;
    size_t array_size;
    struct page **pages;
    struct page *page;

    array_size = (size_t)nr_pages * sizeof(struct page *);

    /* Aloco espaço para o vetor que vai receber os endereços físicos dos frames. */
    if (array_size > PAGE_SIZE)
        pages = vmalloc(array_size);
    else
        pages = kmalloc(array_size);

    if (!pages)
    {
        return NULL;
    }

    area->pages = pages;
    area->nr_pages = nr_pages;
    memset(area->pages, 0, array_size);

    for (size_t i = 0; i < area->nr_pages; i++)
    {
        page = alloc_page(GFP_ZONE_NORMAL);
        if (unlikely(!page))
        {
            /* Successfully allocated i pages, free them in __vfree() */
            area->nr_pages = i;
            // atomic_long_add(area->nr_pages, &nr_vmalloc_pages);
            goto fail;
        }
        area->pages[i] = page;
        // kprintf("\narea->pages=%p - order=%d", area->pages[i], area->pages[i]->level);
    }

    return (virt_addr_t)area->addr;

fail:
    WARN_ERROR("vmalloc: allocation failure: alocados %d de %d bytes", (area->nr_pages * PAGE_SIZE), area->size);
    return NULL;
}

static inline struct vm_struct *find_next_free_area(size_t size, flags_t flags,
                                                    mm_addr_t start, mm_addr_t pend)
{
    struct vm_struct **p, *tmp, *area;
    unsigned long align = 1;
    mm_addr_t addr;

    area = kzalloc(sizeof(*area), GFP_PLACEHOLD);
    if (unlikely(!area))
        return NULL;

    addr = ALIGN(start, align);

    spinlock_lock(&vmalloc_areas.vmlist_lock);
    for (p = &vmalloc_areas.vmlist; (tmp = *p) != NULL; p = &tmp->next)
    {
        if ((unsigned long)tmp->addr < addr)
        {
            if ((unsigned long)tmp->addr + tmp->size >= addr)
                addr = ALIGN(tmp->size +
                                 (unsigned long)tmp->addr,
                             align);
            continue;
        }
        if ((size + addr) < addr)
            goto out;
        if (size + addr <= (unsigned long)tmp->addr)
            goto found;
        addr = ALIGN(tmp->size + (unsigned long)tmp->addr, align);
        if (addr > pend - size)
            goto out;
    }

found:
    area->next = *p;
    *p = area;

    area->flags = flags;
    area->addr = (virt_addr_t)addr;
    area->size = size;
    area->pages = NULL;
    area->nr_pages = 0;
    area->phys_io_addr = 0;

    spinlock_unlock(&vmalloc_areas.vmlist_lock);

    return area;

out:
    spinlock_unlock(&vmalloc_areas.vmlist_lock);
    kfree(area);
    WARN_ERROR("vmalloc: allocation failure: %d bytes", size);
    return NULL;
}

static struct vm_struct *get_vm_area(size_t size, flags_t flags)
{
    struct vm_struct *area = NULL;
    mm_addr_t start = vmalloc_areas.vm_start;
    mm_addr_t pend = vmalloc_areas.vm_pend;

    size = round_up(size, PAGE_SIZE);
    if (!size)
        return NULL;

    if (!(flags & VM_NO_GUARD))
        size += PAGE_SIZE;

    area = find_next_free_area(size, flags, start, pend);

    if (unlikely(!area))
    {
        kprintf("\n(*)%s(%d) - vmalloc: allocation failure: va=%p bytes", __FUNCTION__, __LINE__, area);
        return NULL;
    }

    return area;
}

/**
 * __vmalloc_node_range - allocate virtually contiguous memory
 *
 * @size:		  allocation size
 * @gfp_mask:		  flags for the page level allocator
 * @prot:		  protection mask for the allocated pages
 *
 * Allocate enough pages to cover @size from the page level
 * allocator with @gfp_mask flags.  Map them into contiguous
 * kernel virtual space, using a pagetable protection of @prot.
 *
 * Return: the address of the area or %NULL on failure
 */

static void *__vmalloc_range(size_t size, gfp_t gfp_mask, pgprot_t prot)
{
    struct vm_struct *area;
    size_t real_size = size;

    if (!vmalloc_areas.init)
    {
        WARN_ERROR("vmalloc: no initialized.");
        return NULL;
    }

    size = round_up(size, PAGE_SIZE);

    if (!size || (size >> PAGE_SHIFT) > node_free_pages())
    {
        goto fail;
    }

    /* Faz a verdadeira alocação da área virtual. */
    area = get_vm_area(real_size, VM_ALLOC);
    if (!area)
    {
        WARN_ERROR("vmalloc: __get_vm_area failure: area = %p", area);
        goto fail;
    }

    /* Aloca os frames para mapear o virtual range. */
    if (alloc_vm_pages(area, gfp_mask, prot) == NULL)
    {
        remove_vm_area(area->addr);
        kfree(area);
        return NULL;
    }
    WARN_DEBUGING("Passei alloc_vm_pages.");

    /* Faz o mapeamento da área alocada. */
    map_vm_range((unsigned long)area->addr, get_vm_area_size(area), prot, area->pages);

    return (virt_addr_t)area->addr;

fail:
    WARN_ERROR("vmalloc: allocation failure: %d bytes", real_size);
    return NULL;
}

void *__vmalloc(size_t size, gfp_t gfp_mask, pgprot_t prot)
{
    return __vmalloc_range(size, gfp_mask, prot);
}

/**
 * vmalloc - allocate virtually contiguous memory
 * @size:    allocation size
 *
 * Allocate enough pages to cover @size from the page level
 * allocator and map them into contiguous kernel virtual space.
 *
 * For tight control over page level allocator and protection flags
 * use __vmalloc() instead.
 *
 * Return: pointer to the allocated memory or %NULL on error
 */
void *vmalloc(size_t size)
{
    pgprot_t pgprot = {.value = pgprot_PW};
    return __vmalloc(size, GFP_ZONE_NORMAL, pgprot);
}

/**
 *	vfree  -  release memory allocated by vmalloc()
 *
 *	@addr:		memory base address
 *
 *	Free the virtually contiguous memory area starting at @addr, as
 *	obtained from vmalloc(), vmalloc_32() or __vmalloc().
 *
 *	May not be called in interrupt context.
 */
void vfree(void *addr)
{
    __vunmap(addr, 1);
}
