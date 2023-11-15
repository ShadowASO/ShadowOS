/*--------------------------------------------------------------------------
*  File name:  mm_tmp.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 18-07-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a alocação de
páginas físicas.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "info.h"
#include "mm/mm_tmp.h"
#include "mm/bootmem.h"

/* Table criado em bss para uso no mapeamento temporário de pagetables.
Criado em boot.s. */
extern u64_t p1_table_temp;
static u64_t *tmp_p1_table = NULL;
static size_t idx_entry = 0;

/**
 * @brief Faz o remapeamento de todos memória física, iniciando em 0x00 e
 * cobrindo todo um 1Gb
 *
 */

#define REMAP_MEMORIA_SIZE (MEGA_SIZE * 88)
void remap_kernel(void)
{
    kprintf("\nLinha: %d - REMAP_KERNEL: Remapeando a memoria virtual.", __LINE__);

    create_temporary_pagetable();

    u64_t pml4_ant = __read_cr3();
    u64_t pml4_new = alloc_bootmem_dma();
    u64_t *kernel_base = kernel_ini_vm();

    u64_t *p4 = vmap_temporary_pageframe(idx_entry++, pml4_new);
    memset((void *)p4, 0, PAGE_SIZE);

    // Mapeio o entry 510 para apontar para o próprio table para podermos
    // utilizar técnicas recursivas para mapear as tabelas
    p4[ENTRY_RECURSIVA] = pml4_new | pgprot_PW; // entry 510

    // Calculo o offset(index) do registro da tabela PDP dentro da tabela PML4.
    // Esse cálculo é feito com base no endereço inicial que queremos mapear.
    int p4_offset = p4_index((u64_t)kernel_base);

    //**********    (P3)
    // Aloco um pageframe para a pagetable PDP e depois gravo seu endereço
    // físico na tabela PML4
    u64_t p3_frame = alloc_bootmem_dma();
    p4[p4_offset] = ((p3_frame) | pgprot_PW);
    u64_t *p3 = vmap_temporary_pageframe(idx_entry++, p3_frame);
    memset((void *)p3, 0, PAGE_SIZE);

    // Calculo o offset(index) do registro da tabela P2 dentro da tabela P3.
    // Esse cálculo é feito com base no endereço inicial que queremos mapear.
    int p3_offset = p3_index((u64_t)kernel_base);

    //**********    (P2)
    // Aloco um pageframe para a pagetable PDP e depois gravo seu endereço
    // físico na tabela PDP
    u64_t p2_frame = alloc_bootmem_dma();
    p3[p3_offset] = ((p2_frame) | pgprot_PW);

    // Mapeio o novo frame e obtenho um ponteiro para navegar nos registros
    // da tabela PDP. Os pageframes dos pagetables são sempre limpos
    u64_t *p2 = vmap_temporary_pageframe(idx_entry++, p2_frame);
    memset((void *)p2, 0, PAGE_SIZE);

    // Calculo o índice inicial na tabela e o número de registros(entries)
    size_t pd2_offset = p2_index((u64_t)kernel_base);
    size_t p2_entries = p2_entry_len(REMAP_MEMORIA_SIZE);

    //*********    PAGE TABLE - PT1   ****************** /
    /**
     * @brief Crio uma tabela PT para cada registro de PD. Na rotina seguinte estas
     * tabelas serão preenchidas com os endereços de pageframes
     * vou mapear 2 M
     */

    for (size_t i = 0; i < p2_entries; i++)
    {
        p2[pd2_offset + i] = (alloc_bootmem_dma() | pgprot_PW);
    }

    /*
    Fazemos o mapeamento de toda a memória física, até 1Gb, iniciando em 0x0
    */

    u64_t physic_addr = 0x0;
    page_table_t *page_pt = (page_table_t *)p2; // Representa PT e Receberá o endereço de cada frame da memória
    page_table_t page_tmp;                      // receberá os registro de PD
    page_entry_t frame_tmp;                     // Recebe o endereço de cada frame de PT e atributos

    /**
     * @brief A tabela page_tmp é utilizada para copiar todas as entradas da tabela PD.
     * Na rotina anterior, foram criadas tabelas PT para cada um dos registros de PD.
     * Agora, nós vamos mapear 512 pageframes para cada uma das 512 tabelas PT.
     *
     */

    memset(&page_tmp, 0, PAGE_SIZE); // limpo a tabela temporária
    memcpy(&page_tmp, p2, PAGE_SIZE);

    u64_t max_mem = 0;

    for (size_t i = 0; i < p2_entries; i++)
    {

        // Agora mapeamos uma page PT para cada registro de PD e fazemos o preenchimento de cada
        // entry com os endereços físicos de cada frame da memória.
        // page_pt = kmap_temporary_page(page_tmp.entry[pd2_offset + i].value, flags_mmap);
        page_pt = vmap_temporary_pageframe(idx_entry, page_tmp.entry[pd2_offset + i].value);
        memset((void *)page_pt, 0, PAGE_SIZE);

        // Atravessa todas as tabelas PT, inserindo os endereços de frames(entries)
        for (int x = 0; x < PTRS_PER_PGTABLE; x++)
        {

            frame_tmp.value = physic_addr;

            if (frame_tmp.value >= ((u64_t)&_rodata_begin_phys) &&
                frame_tmp.value < ((u64_t)&_rodata_end_phys))
            {
                // Somente leitura - limpa bit (Write)
                // Nao permite execução - seta bit 63
                frame_tmp.pte.flags.RW = 0;
                frame_tmp.pte.flags.NX = 1;
            }
            // TEXT = Código executável
            else if (frame_tmp.value >= ((u64_t)&_text_begin_phys) &&
                     frame_tmp.value < ((u64_t)&_text_end_phys))
            {
                // Somente leitura - limpa bit (Write)
                // Permite execução - limpa bit 63
                frame_tmp.pte.flags.RW = 0;
                frame_tmp.pte.flags.NX = 0;
            }
            else
            {
                frame_tmp.pte.flags.RW = 1;
            }

            //  Todos os frames Present e Global
            frame_tmp.pte.flags.P = 1;
            frame_tmp.pte.flags.G = 1;

            page_pt->entry[x].pte = frame_tmp.pte;
            physic_addr = physic_addr + PAGE_SIZE;
            max_mem = max_mem + PAGE_SIZE;
        }
    }

    kunmap_temporary_page();
    __write_cr3(pml4_new);
    // DBG_PAUSE("\\npassei");
}

/**
 * @brief Recebe o endereço de um frame de memória e o mapeia provisoriamente
 * para a realização de modificações, tais como limpeza. O mapeamento é reali-
 * zado na pml4_table[0].
 *
 * @param frame_addr
 * @return u64_t* - Retorna um endereço virtual que permite modificar o
 * frame.
 */
void *kmap_temporary_page(u64_t frame_addr, u64_t flags)
{
    kunmap_temporary_page();
    frame_addr = (u64_t)align_down(frame_addr, PAGE_SIZE);

    u64_t *pml4_address = (u64_t *)MASK_RECURSIVE_P4;
    pml4_address[0] = (frame_addr | pgprot_PW);

    u64_t *page_address = (u64_t *)MASK_RECURSIVE_P3;
    // u64_t page_address = MASK_RECURSIVE_P3;
    //  faz um flush na TLB - Isso gerou muitos erros, pois a TLB estava retendo o
    //  endereço da tabela anteriormente mapeada.

    //__vm_invlpg_tlb(page_address);
    __vm_invlpg_tlb((u64_t)page_address);

    return (void *)page_address;
}
/**
 * @brief Cria uma pagetable(p1) temporário para permitir que os seus
 * entries sejam utilizados para mapear frames que serão modificados,
 * principalmente para criar pagetables
 *
 */
void create_temporary_pagetable(void)
{
    kunmap_temporary_page();
    // Zero o índice do entry dentro da tabela temporária.
    idx_entry = 0;

    // Usa tabela criada em bss pelo boot.s
    u64_t tmp_pageframe = (u64_t)&p1_table_temp;

    u64_t *pml4 = (u64_t *)MASK_RECURSIVE_P4;
    pml4[0] = tmp_pageframe | PG_FLAG_P | PG_FLAG_W;

    tmp_p1_table = (u64_t *)MASK_RECURSIVE_P3;
    memset((void *)tmp_p1_table, 0, PAGE_SIZE);

    __vm_invlpg_tlb((u64_t)pml4);
}
/**
 * @brief Faz o mapeamento temporário de até 512 pageframes simultaneamente
 * deve ser utilizada juntamente com void create_temporary_pagetable(void).
 * @param idx
 * @param addr
 * @return void*
 */
void *vmap_temporary_pageframe(size_t idx, u64_t addr)
{
    page_entry_t p4_entry = (page_entry_t) * (u64_t *)MASK_RECURSIVE_P4;

    if (idx >= PTRS_PER_PGTABLE)
    {
        kprintf("\nidx=%d: valor deve ser menor que 512", idx);
        return NULL;
    }

    if (!p4_entry.p4e.flags.P)
    {
        kprintf("\nFalta criar o pagetable temporário:\ncreate_temporary_pagetable(void)");
        return NULL;
    }

    u64_t entry = (u64_t)(align_down(addr, PAGE_SIZE));
    tmp_p1_table[idx] = (u64_t)entry | PG_FLAG_P | PG_FLAG_W;

    u64_t *virt_addrs_pageframe = (u64_t *)(MASK_RECURSIVE_P2 | (idx << P1_SHIFT));

    //  faz um flush na TLB - Isso gerou muitos erros, pois a TLB estava retendo o
    //  endereço da tabela anteriormente mapeada.
    __vm_invlpg_tlb((u64_t)virt_addrs_pageframe);

    return (u64_t *)virt_addrs_pageframe;
}
/**
 * @brief Desfaz o mapeamento feito com kmap_temporary_page, retirando o mapeamento
 * feito para pml4_table[0]
 *
 */
void kunmap_temporary_page()
{
    u64_t *entry_address = (u64_t *)MASK_RECURSIVE_P4;
    entry_address[0] = 0;
    // u64_t *page_address = (u64_t *)MASK_RECURSIVE_P3;

    //   faz um flush na TLB - Isso gerou muitos erros, pois a TLB estava retendo o
    //   endereço da tabela anteriormente mapeada.
    __vm_invlpg_tlb((u64_t)entry_address);
}

/**
 * @brief Faz uma limpeza em um frame de memória passado para ele.
 *
 * @param frame_addr
 */
void clear_frame_tmp(u64_t frame_addr)
{
    kunmap_temporary_page();

    if (frame_addr & 0xFFF)
    {
        frame_addr = (frame_addr | 0xFFF) ^ 0xFFF;
    }
    u64_t *virtual_address = kmap_temporary_page(frame_addr, pgprot_PW);
    memset((void *)virtual_address, 0, PAGE_SIZE);

    kunmap_temporary_page();
}