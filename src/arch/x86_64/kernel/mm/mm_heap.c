/*--------------------------------------------------------------------------
*  File name:  heap.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 16-08-2020
*--------------------------------------------------------------------------
Este header reune as rotinas para a inicialização da memória heap.
--------------------------------------------------------------------------*/

#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "stdio.h"
#include "mm/mm_heap.h"
#include "list.h"
#include "mm/kmalloc.h"
#include "string.h"
#include "debug.h"
#include "mm/vmm.h"
#include "mm/vm_area.h"

mm_heap_t kmm_heap;
vm_area_t vmm_area_heap;

static void init_heap(void)
{
    kprintf("\n%s(%d) - Inicializando a heap.", __FUNCTION__, __LINE__);

    kmm_heap.vm_start = (virt_addr_t)get_virt_heap();
    kmm_heap.heap_size = 0;
    kmm_heap.vm_pend = kmm_heap.vm_start;
    kmm_heap.heap_free = 0;
    kmm_heap.max_size = MAX_HEAP_SIZE;

    kprintf("\n%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}

static void maping_heap(virt_addr_t start, virt_addr_t pend)
{
    kprintf("\n%s(%d) - Mapeando memória virtual para a heap.", __FUNCTION__, __LINE__);

    vm_area_t *vmm = &vmm_area_heap;

    vmm->flags = VM_HEAP;
    vmm->pgprot.value = pgprot_PW;
    vmm->vm_start = start;
    vmm->vm_pend = pend;
    kmmap(vmm);

    // kprintf("\nvmm->vm_start=%p - vmm->vm_pend=%p", vmm->vm_start, vmm->vm_pend);

    kprintf("\n%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}

/**
 * Disponibiliza páginas da memória virtual para serem administradas
 * pelo alocador de memória
 */
void brk(size_t bck_bytes)
{
    freeHead_t *start = (freeHead_t *)heap_pend();

    /* Reconfigura a estrutura kmm_heap e realoca o healp_pend para a nova localização. */
    heap_update_add(bck_bytes);
    maping_heap(start, heap_pend());

    /* O novo pend será utilizado para controlar o limite da memória. */
    freeHead_t *pend = (freeHead_t *)heap_pend();

    while (start < pend)
    {
        start->level = calc_bin_order(PAGE_SIZE);
        start->status = eBUDDY_FREE;

        // kprintf("\nnr_bcks=%d - start=%p start->level=%d - pend=%p", nr_bcks, start, start->level, pend);

        __brk(start);

        start = incptr(start, PAGE_SIZE);
    }
}
/**
 * @brief Responsável pela inicialização e criação da heap do kernel
 * ATENÇÃO - esta rotina precisa ser adaptada, pois não funcionará.
 *
 */
void setup_heap(void)
{
    init_heap();

    init_buddy_free_lists();

    /* Atribuo o primeiro bloco à heap. */

    brk(MIN_BLOCK_SIZE);
}

void sbrk(size_t heap_lim)
{
    // kprintf("\nsbrk - Linha: %d - heap_lim=%x", __LINE__ ,heap_lim);

    // mm_heap_head.size = heap_lim;
    // mm_heap_head.vm_pend = (virt_addr_t)align_up(((u64_t)mm_heap_head.vm_start + mm_heap_head.size), PAGE_SIZE);
}
