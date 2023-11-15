/*--------------------------------------------------------------------------
*  File name:  bootmem.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 20-07-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados à alocação e admi-
nistração da memória durante o boot inicial, antes que as estruturas defi-
nitivas tenham sido criadas.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "mm/mm.h"
#include "mm/mm_types.h"
#include "mm/page.h"
#include "info.h"
#include "multiboot2.h"
#include "mm/page_alloc.h"
#include "mm/kmalloc.h"
#include "mm/bitmap.h"
#include "string.h"
#include "debug.h"
#include "stdlib.h"
#include "mm/bootmem.h"
#include "mm/tlb.h"
#include "sysinfo.h"
#include "mm/vm_area.h"
#include "x86.h"

/**
 * @brief Aqui eu calculo o tamanho e crio uma matriz de bits para
 * gerenciar a memória identificada no boot, até que as estruturas
 * definitivas estejam criadas.
 *
 */

node_t pgdat = {0};
vm_area_t vmm_system = {0};
vm_area_t vmm_temp = {0};

/**
 * @brief  Extrai as informações sobre a memória física, a partir dos dados
 * fornecidos pelo bootloader. Depois as insere na estrutura mmap_regions.
 * Com essa estrutura, podemos fazer a montagem das demais estruturas de controle
 * e gerenciamento de memória.
 * @note
 * @retval None
 */
static void load_multiboot_physic_mmapa(void)
{
    kprintf("\n(*)%s(%d) - Ini. MULTIBOOT_MAPA: Mapeando a memoria.", __FUNCTION__, __LINE__);

    struct multiboot_tag *tag;
    multiboot_memory_map_t *mmap;
    size_t tmp_frame = 0ULL;
    u64_t mmap_len = 0;

    /*Parte fixa
      u32 total_size; //Tamanho bytes ocupado pelas informações do bootloader
      u32 reserved;
    */
    struct multiboot_info info = *(struct multiboot_info *)phys_to_virt(hw_info.mboot_addr);
    size_t mbi_size = info.total_size;

    /*Tags
      u32 type;
      u32 size;
    */
    struct multiboot_tag *tags = phys_to_virt(hw_info.mboot_addr + 8);

    pgdat.mmap_regions.count = 0;

    for (tag = (tags);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *)((multiboot_uint8_t *)tag + ((tag->size + 7) & ~7)))
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
        {
            size_t i = 0; // ATENÇÃO - Necessária para mapear as regiões
            for (mmap = ((struct multiboot_tag_mmap *)tag)->entries;
                 (multiboot_uint8_t *)mmap < (multiboot_uint8_t *)tag + tag->size;
                 mmap = (multiboot_memory_map_t *)((u64_t)mmap + ((struct multiboot_tag_mmap *)tag)->entry_size))
            {
                //----------------------------------
                pgdat.mmap_regions.region[i].base = mmap->addr;
                pgdat.mmap_regions.region[i].size = mmap->len;
                pgdat.mmap_regions.region[i].len_frames = (mmap->len / PAGE_SIZE);
                pgdat.mmap_regions.region[i].flags = mmap->type;
                pgdat.mmap_regions.max = mmap->len;

                if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE)
                {
                    kprintf("\nBase: %x | Length: %x | Frames: %d | Type: %d - (Free)",
                            mmap->addr, mmap->len, (pgdat.mmap_regions.region[i]).len_frames, mmap->type);
                }
                else
                {
                    kprintf("\nBase: %x | Length: %x | Frames: %d | Type: %d - Res",
                            mmap->addr, mmap->len, pgdat.mmap_regions.region[i].len_frames, mmap->type);
                }
                i++; // ATENÇÃO - Necessária para mapear as regiões
                pgdat.mmap_regions.count++;
            }
        }
    }
    kprintf("\n\nMBI size: tamanho ocupado pelo multiboot - %d bytes\n", mbi_size);
}
/**
 * @brief Calcula os limites da memória, a partir das informações inseridas na es-
 * trutura mmap_region_t. Elas são inseridas no próprio node do kernel. Posterior-
 * mente é feito o complementos dessa configuração do "node".
 * Neste ponto também é feito o preenchimento da estrutura "bootmem"
 *
 */
static void load_bootmem_physic_mlimits(void)
{
    kprintf("\n(*)%s(%d) - BOOTMEM_LIMITS: Mapeando limites da memoria.", __FUNCTION__, __LINE__);

    u64_t region_base = 0;
    u64_t region_size = 0;
    uint8_t flags = 0;

    phys_addr_t ini = 0;
    phys_addr_t end = 0;
    size_t size = 0;
    phys_addr_t pend_max = 0;

    for (size_t i = 0; i < pgdat.mmap_regions.count; i++)
    {
        region_base = pgdat.mmap_regions.region[i].base;
        region_size = pgdat.mmap_regions.region[i].size;
        flags = pgdat.mmap_regions.region[i].flags;

        end = round_down((region_base + region_size), PAGE_SIZE);
        ini = round_up(region_base, PAGE_SIZE);
        size = end - ini;

        if (flags == MMAP_AVAILABLE)
        {
            pend_max = __max_uint64_val(pend_max, end);
        }
    }

    pgdat.mmap_regions.mm_start = 0;
    pgdat.mmap_regions.mm_size = pend_max;
    pgdat.mmap_regions.pg_free = 0;
    pgdat.mmap_regions.pg_total = (pend_max / PAGE_SIZE) - 1;
    pgdat.mmap_regions.max_pfn = (pend_max / PAGE_SIZE) - 1;

    // show_struct_mmap_region();
    // show_struct_bootmem();
    // debug_pause("p1");
}

/**
 * @brief Faz o setup das estruturas necessárias para o gerenciamento de memória
 * antes das estruturas definitivas estarem concluídas. Cria a matriz de bits
 * para gerenciar a memória livre.
 *
 * @return phys_addr_t
 */
static void setup_bootmem_mbitmap(void)
{
    kprintf("\n(*)%s(%d) - Fazendo o setup do bootmem e da matriz de bits.", __FUNCTION__, __LINE__);

    /* Fazemos a inicialização do bitmap com a quantidade de frames(max_pfn) e
    obtemos o cálculo do tamanho do bitmap em bytes.*/

    /* Posição inicial do bitmap na memória física deve ser a partir do PFN=1.
    O frame PFN=0 não deve ser utilizado/modificado, pois gera erros. */

    /* O vetor de interrupções do modo real(IVT) fica localizado nos primeiro
    1024(0x400) bytes da memória física. Se for sobrescrito, perdermos seus
    valores, o que  gera erro no momento que acionamos as interrupções da BIOS/VESA.
    Isso ocorre porque o código vesa_tramp aciona interrupções. Isso já não
    ocorre com o código do tramp(SMP), pois não disparamos interrupções.
    Por isso, colocamos a matriz de bits do bootmem no enderesso físico
    0x1000. */

    phys_addr_t start = get_phys_bootmem_bitmap_offset();

    /* Faço a inicialização da estrutura de controle do bitmap, atribuindo
    o endereço inicial do bloco de memória para acomodar a matriz de bits. */

    bitmap_init(&pgdat.bdata.bitmap_mem, phys_to_virt(start), pgdat.mmap_regions.max_pfn);

    size_t bitmap_bits = pgdat.bdata.bitmap_mem.nr_elem;
    size_t bitmap_size = pgdat.bdata.bitmap_mem.size;

    /*Preencho toda a matriz de bits com o binário "1", para  tornar todas os
    frames da memória reservados. Posteriormente serão reativados aqueles que
    estão livres. Esse trabalho será feito pelo register_bootmem_frames_free(). */

    bitmap_set_all(&pgdat.bdata.bitmap_mem);

    kprintf("\n(*)%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}
/**
 * @brief Faz a configuração da estrutura do "node", inicializando a estrutura
 * de bitmap que faz o gerenciamento inicial da memória física. Faz também  o
 * cálculo dos valores iniciais do "node". Ao concluir, temos uma matriz de bits
 * que identifica todas as páginas "reservadas" e "livres". Nas áreas reservadas
 * estão incluídas as áreas ocupadas pelo kernel.
 *
 */
static void register_bootmem_frames_free(void)
{
    kprintf("\n(*)%s(%d) - Configurando o node.", __FUNCTION__, __LINE__);

    size_t start_pfn;
    size_t pend_pfn;

    //**** bootmem

    pgdat.bdata.node_start_pfn = 0; /* Apesar de iniciar no pfn(frame)=0 onde fica a (IVT), uso inicia em pfn=1. */
    pgdat.bdata.node_last_pfn = pgdat.mmap_regions.max_pfn;

    /* Uso do bootmem_allocator. */
    pgdat.bdata.last_alloc_pfn = phys_to_pfn(kernel_pend()) - 1;
    pgdat.bdata.last_alloc_offset = 0;

    /* Localização e limite da matriz de bits(BITMAP).*/

    size_t pfn_bitmap_ini = phys_to_pfn(virt_to_phys(pgdat.bdata.bitmap_mem.data));
    size_t pfn_bitmap_pend = phys_to_pfn(virt_to_phys(pgdat.bdata.bitmap_mem.data) + pgdat.bdata.bitmap_mem.size) + 1;

    /* Localização e limite do kernel. */

    size_t pfn_kernel_ini = phys_to_pfn(align_down(kernel_ini(), PAGE_SIZE));
    size_t pfn_kernel_pend = phys_to_pfn(align_up(kernel_pend(), PAGE_SIZE));

    /*---------------------------------------------------------------*/
    pgdat.node_free_pages.value = 0; /* Não sabemos ainda quantos frames estão livre. */

    /* Mapeamos a memória livre no BITMAP, cujos bits foram todos marcados como em uso==1.
    Para isso, fazemos uma varredura nas regiões da memória, sinalizando os bits corres-
    pondentes aos frames livres. */

    for (size_t i = 0; i < pgdat.mmap_regions.count; i++)
    {

        if (pgdat.mmap_regions.region[i].flags == MMAP_AVAILABLE)
        {
            start_pfn = phys_to_pfn(align_up(pgdat.mmap_regions.region[i].base, PAGE_SIZE));
            pend_pfn = phys_to_pfn(align_down(pgdat.mmap_regions.region[i].base + pgdat.mmap_regions.region[i].size, PAGE_SIZE));

            for (size_t pfn = start_pfn; pfn < pend_pfn; pfn++)
            {

                /* O pfn==0 NÃO PODE ser utilizado. Gera erros. */
                if (pfn == 0)
                {
                    /* faz nada */
                }
                /* os frames ocupados pelo kernel não podem ser utilizados. */
                else if ((pfn >= pfn_kernel_ini) && (pfn < pfn_kernel_pend))
                {
                    /*faz nada */
                }
                /* Os frames utilizados pelo bitmap do bootmem não devem ser utilizados. */
                else if ((pfn >= pfn_bitmap_ini) && (pfn < pfn_bitmap_pend))
                {
                    /* faz nada. */
                }
                else
                {
                    atomic64_inc(&(pgdat.node_free_pages));
                    free_bootmem_frame(pfn); // Sinalizo na matriz de bits "bitmap"
                }
            }
        }
    }

    /* Reservo memória física para o vetor de pages. */
    pgdat.nr_zones = MAX_ZONE_MEMORY;
    pgdat.node_start_pfn = pgdat.bdata.node_start_pfn; /* pfn=0*/
    pgdat.node_last_pfn = pgdat.bdata.node_last_pfn;   /* pfn=MAXMEM */
    /*----------------------------------------------------------------------*/

    // show_struct_mmap_region();
    // show_struct_bootmem();

    kprintf("\n(*)%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}

/**
 * -----------------------------------------------------------------------
 * Reserva e limpa a memória física contínua que será ocupada pelo
 * pagemap vector.
 *
 * Com a estrutura de page_t concluída, já podemos utilizá-la para
 * gerenciar a memória física, configurando cada elemento page_t
 * com a informação de free/reservado da matriz bitmap.
 *------------------------------------------------------------------------
 */
static void setup_pagemap_vector(void)
{
    kprintf("\n(*):%s(%d) - Inicializando e Configurando o vetor pagemap(pate_t).", __FUNCTION__, __LINE__);

    /* Offset da posição inicial do vetor na memória física. */
    // pgdat.node_page_map = phys_to_virt(get_phys_memmap_offset()); /* memmap==Estruturas de page_t.*/
    pgdat.node_page_map = get_virt_memmap_offset(); /* memmap==Estruturas de page_t.*/

    size_t map_size = pgdat.node_last_pfn * sizeof(page_t);
    // phys_addr_t map_ini = (virt_to_phys(pgdat.node_page_map));
    phys_addr_t map_ini = get_phys_memmap_offset();
    phys_addr_t map_end = round_up(map_ini + map_size, PAGE_SIZE);

    /* Faz a limpeza inicial do vetor mem_map. */
    kprintf("\n(*):%s(%d) - Criando estrutura memmap para memoria fisica.", __FUNCTION__, __LINE__);

    kprintf("\n\npgdat.node_page_map=%p", pgdat.node_page_map);
    kprintf("\npgdat.pgdat.node_last_pfn=%p", pgdat.node_last_pfn);
    kprintf("\npage_map size=%x", map_size);

    /* Reservo memória física NO BITMAP para o vetor de pages. Isso impedirá
    que os frames do vetor de page_t sejam inseridos no buddy allocator como
    livres.
    */
    // kprintf("\nphys_to_pfn(map_ini)=%x-> phys_to_pfn(map_end)=%x", phys_to_pfn(map_ini), phys_to_pfn(map_end));
    // debug_pause("P1");

    reserve_bootmem_frame_ranger(phys_to_pfn(map_ini), phys_to_pfn(map_end));

    kprintf("\n(*):%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}
/**
 * @brief Inicializa a estrutura init_mm para ser utilizada no remapeamento da memória
 * ainda em desenvolvimento.
 *
 */
static void config_init_mm(void)
{
    // phys_addr_t pt4 = __read_cr3();
    phys_addr_t pt4 = read_cr3();
    phys_addr_t pt_swap = swapper_pt4_dir();
    memcpy(phys_to_virt(pt_swap), phys_to_virt(pt4), PAGE_SIZE);

    init_mm.pml4 = phys_to_virt(swapper_pt4_dir());
    memset(init_mm.pml4, 0, PAGE_SIZE);

    init_mm.mmap = NULL;

    init_mm.phys_map_ini = 0;
    // init_mm.phys_map_end = 0;

    init_mm.next_map_mem = 0;

    init_mm.mmap_base = kernel_ini_vm();
    init_mm.virt_map_ini = (void *)get_virt_page_offset();
    init_mm.virt_map_end = kernel_end_vm();
    init_mm.nr_frames_mapped = 0;

    // mmap_base;
    init_mm.nr_frames_mapped = 0;
    // init_mm.map_size = 0;

    init_mm.map_count = 1;
    init_mm.mm_users = 1;
    init_mm.mm_count = 1;

    init_mm.start_rodata = rodata_begin_k();
    init_mm.end_rodata = rodata_end_k();

    init_mm.start_code = text_begin_k();
    init_mm.end_code = text_end_k();

    init_mm.start_data = data_begin_k();
    init_mm.end_data = data_end_k();

    init_mm.start_bss = bss_begin_k();
    init_mm.end_bss = bss_end_k();

    init_mm.start_brk = 0;
    init_mm.end_brk = 0;

    init_mm.start_stack = kernel_stack();
    init_mm.arg_start = 0;
    init_mm.arg_end = 0;

    init_mm.env_start = 0;
    init_mm.env_end = 0;
}

/**
 * -------------------------------------------------------------------------------
 * Mapeia toda a memória física do sistema, colocando os PAGETABLE na ZONA NORMAL,
 * acima do espaço da ZONA_DMA(16M).
 *
 * O vetor de pate_t(map_page) será colocado no início da ZONA NORMAL. A reserva
 * é feita no bitmap.
 *
 * A memória na ZONA_DMA(16M) receberá os pagetables do mapeamento temporário da
 * mémória, utilizando uma área contínua logo acima do kernel.
 *
 *
 * O PML4 utilizado no sistema é um frame reservado de forma fixa no kernel e  pode
 * ser referenciado pela variável (_swapper_pt4_dir) do script de linkagem.
 *
 * LAYOUT:
 *
 * |---------------------------------------------------------------------------|
 * | (0x0)     | pfn:0x200<-->pfn:0x260 |           | pfn:0x1000
 * |---------------------------------------------------------------------------|
 * |   FREE    |  kernel                | DMA       | page_map - ZONA_NORMAL
 * |---------------------------------------------------------------------------|
 *
 * Após o mapeamento definitivo da memória física do sistema, faremos a criação
 * do vetor de page_t, utilizado para referenciar e gerenciar o frames do siste-
 * ma.
 * --------------------------------------------------------------------------------
 */

static void remap_full_memory_system(void)
{
    kprintf("\n(*)%s(%d) - Fazendo a remap definitivo da memory.", __FUNCTION__, __LINE__);

    phys_addr_t cr3 = __read_cr3();

    phys_addr_t pt_tmp = alloc_bootmem_dma();
    clear_frame(phys_to_virt(pt_tmp));
    memcpy(phys_to_virt(pt_tmp), phys_to_virt(cr3), PAGE_SIZE);
    __write_cr3(pt_tmp);

    /* -----------------------------------------------*/
    phys_addr_t pt_swap = swapper_pt4_dir();
    clear_frame(phys_to_virt(pt_swap));
    init_mm.pml4 = phys_to_virt(pt_swap);

    /*
    ----------------------------------------------------------------------
                            ( FULL MEMORY )
    ----------------------------------------------------------------------
    Configura toda a memória, iniciando no offset 0x80000000 ....
        vm_ini(0x80000000) ------------>vm_pend(+ physic mem_size)
    ----------------------------------------------------------------------
    */

    virt_addr_t vm_ini = (u64_t *)get_virt_page_offset();
    size_t vm_size = (pgdat.node_last_pfn * PAGE_SIZE);

    vm_area_t *vm_area = &vmm_system;
    vm_area->flags = VM_ZONE_DMA | VM_RANGER | VM_FULL;
    vm_area->pgprot.value = PG_FLAG_P | PG_FLAG_W | PG_FLAG_U; //(*)Excluir PG_FLAG_U
    vm_area->vm_start = vm_ini;
    vm_area->vm_pend = incptr(vm_ini, vm_size);
    vm_area->phys_base = pfn_to_phys(pgdat.node_start_pfn);

    mmap_bootmem(vm_area);

    /* Mapeio o vetor de pages. */
    vm_ini = (u64_t *)get_virt_memmap_offset();

    size_t map_size = pgdat.node_last_pfn * sizeof(page_t);

    // phys_addr_t map_ini = (virt_to_phys(pgdat.node_page_map));
    // phys_addr_t map_ini = get_phys_memmap_offset();
    // phys_addr_t map_end = round_up(map_ini + map_size, PAGE_SIZE);
    vm_size = round_up(map_size, PAGE_SIZE);

    vm_area->flags = VM_ZONE_DMA | VM_RANGER;
    vm_area->pgprot.value = PG_FLAG_P | PG_FLAG_W;
    vm_area->vm_start = vm_ini;
    vm_area->vm_pend = incptr(vm_ini, vm_size);
    vm_area->phys_base = get_phys_memmap_offset();

    mmap_bootmem(vm_area);

    /* Gravo o novo pagetable(PML4) no registro cr3. */
    //__write_cr3(pt_swap);

    /*------------------------------------------------------------------
                       MEMORY IDENTITY - 2 MEGA
    Mapeia Primeiro Mega de memória física na forma Identiti.
    --------------------------------------------------------------------*/

    vm_area->flags = VM_ZONE_DMA | VM_IDENTI; /* A partir de 16M.*/
    vm_area->vm_start = 0x0;
    vm_area->vm_pend = (virt_addr_t)(MEGA_SIZE * 2);
    vm_area->phys_base = 0;

    mmap_bootmem(vm_area);
    /* Gravo o novo pagetable(PML4) no registro cr3. */
    __write_cr3(pt_swap);

    /* OBS: É preciso desenvolver alguma rotina para liberar os frames
    alocados anteriormente no mapeamento temporário. */

    kprintf("\n(*)%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}

/**
 * -----------------------------------------------------------------------
 * Faz a inicialização do vetor pagemap, utilizando o free/used do bitmap.
 *
 * Inicializa as listas do buddy allocator.
 *
 * É feita também a identificação de alguns pageframes "reservados" pelo
 * hardware para mapear APIC, IOAPIC, HPE etc. Eles recebem o atributo
 * "fixmap" no seu flags.
 * -----------------------------------------------------------------------
 */
static void free_area_init(void)
{
    zone_t *zone;
    page_t *page;
    u8_t zone_id;

    /* Limpo a memória física do pagemap vector. */
    memset(pgdat.node_page_map, 0, pgdat.node_last_pfn * sizeof(page_t));

    /*---------------------------------------------------------------*/
    pgdat.node_free_pages.value = 0; /* Faremos uma nova contagem de frames livre. */

    /**
     * @brief Atavessa todo o mem_map e complemento a configuração de
     * cada "struct page", inserindo nas listas do buddy allocator
     * aquelas que estão sinalizadas como livres.
     * Faço também a identificação de alguns pageframes "reservados" pelo
     * hardware para mapear APIC, IOAPIC, HPE etc. Eles recebem o atributo
     * "fixmap" no seu flags.
     *
     */

    for (size_t pfn = 0; pfn < pgdat.node_last_pfn; pfn++)
    {
        zone_id = pfn_to_zoneid(pfn);
        zone = zone_obj(zone_id);
        page = &pgdat.node_page_map[pfn];

        set_block_level(page, 0);     // Um frame corresponde ao order 0(zero)
        set_page_zone(page, zone_id); // Atribuo o número da zona. Será utilziada pelo buddy

        set_page_present(page);

        /**
         * @brief Verifico se o pageframe já foi utilizado, durante
         * a inicialização do sistema, quando as rotinas usam  o
         * bitmap(pglist_data.bitmap_mem) para administrar a memória.
         * Estes frames ficam marcados como reservados
         */
        if (is_bootmem_frame_used(pfn))
        {
            set_page_used(page);
        }
        else if (is_page_system_fixmaped(pfn))
        {
            set_page_fixmap(page);
        }
        else
        {
            //__free_pages(page);
            free_pages(page);
        }
    }

    kprintf("\n(*):%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}

void init_bootmem(void)
{

    kprintf("\n(*)%s(%d) - INIT_BOOTMEM: Mapeando a memoria.", __FUNCTION__, __LINE__);

    /**
     * Faz o mapeamento das áreas de memória, configurando as estrutura
     * mmap_region_t a partir dos dados fornecidos pelo bootloader.
     */
    load_multiboot_physic_mmapa();

    /* Calculo as propriedades da memória disponível. */
    load_bootmem_physic_mlimits();

    /**
     * Preparo a estrutura do bootmem para a alocação de memória,
     * criando a matriz de bits.
     */
    setup_bootmem_mbitmap();

    /**
     * Configura a memória livre, sinalizando no btmap do bootmem.
     * Esta matriz de bits será utilizada para a criação da estrutura
     * mempage(page_t).
     */
    register_bootmem_frames_free();

    /* Reserva espaço de memória física para o pagemap, no bitmap. */
    setup_pagemap_vector();

    /**
     * ------------------------------------------------------------
     *  *** PARTIR deste ponto, posso utilizar o alloc_bootmem() ***
     * ------------------------------------------------------------
     */

    /* Faço a inicialização do init_mm(mm_struct). */
    config_init_mm();

    /**
     * Faz o mapeamento inicial da memória a apartir do bootmem, criando
     * a área virtual necessária ao kernel e ao vetor de struct page.
     * */
    // remap_full_memory_temp();

    remap_full_memory_system(); // pagetables 16M --> mem_fullsize

    /* Configura as zonas e suas freelists*/
    setup_zone_free_list();

    /* Faço a inclusão dos pages nas listas livres do buddy. */
    free_area_init();

    /**
     * --------------------------------------------------------------------------
     * A partir daqui não se pode mais utilizar o alloc_bootmem ou alloc_bootmem,
     * pois toda a memória está mapeada pelas estruturas de struct page.
     * --------------------------------------------------------------------------
     * */

    kprintf("\n(*)%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}

/**
 * @brief Allocator do bootmem simples para inicializar o sistema.
 */
static phys_addr_t alloc_bootmem(size_t size_bck)
{
    size_t start_pfn = pgdat.bdata.last_alloc_pfn;
    u32_t offset = pgdat.bdata.last_alloc_offset;
    phys_addr_t mm_offset = 0;
    bool alloc = false;

    /*---------------------------------------------------------------------
    Será necessário alocar novo(s) frames:
    a) Primeira chamada da rotina(offset==0);
    b) bloco maior que PAGE_SIZE;
    b) bloco não cabe no espaço restante do último frame alocado - offset + bloco > PAGE_SIZE
    */
    if (size_bck > PAGE_SIZE)
    {
        alloc = true;
    }
    else if (align_up((offset + size_bck), 64) > PAGE_SIZE)
    {
        alloc = true;
    }
    else if (offset == 0)
    {
        alloc = true;
    }
    /* ------------------------------------------*/
    if (alloc)
    {
        start_pfn = alloc_bootmem_frames_bck(size_bck, start_pfn);
        offset = 0;
    }

    /* Posição na memória .*/
    mm_offset = pfn_to_phys(start_pfn) + offset;

    /* Calculamos os novos valores do last_alloc_pfn e last_alloc_offset. */
    phys_addr_t next_offset = align_up((mm_offset + size_bck), 64);

    pgdat.bdata.last_alloc_pfn = phys_to_pfn(align_down(next_offset, PAGE_SIZE));
    pgdat.bdata.last_alloc_offset = next_offset - pfn_to_phys(pgdat.bdata.last_alloc_pfn);

    return mm_offset;
}

static inline phys_addr_t bootmem_map_p3(p4e_t *p4e, pgprot_t pg_prot, vm_flags_t flags)
{
    phys_addr_t pgframe = 0;

    /* Se o pagetable já está mapeado, devolve o seu endereço físico. */
    if (p3_present(p4e))
    {
        pgframe = MASK_PAGE_ENTRY((*p4e).p4e);
    }
    /* Aloca um novo pageframe e insere seu endereço físico no entry. */
    else
    {
        pgframe = alloc_bootmem_pagetable(flags);
        clear_frame(phys_to_virt(pgframe));
        (*p4e).p4e = pgframe | pg_prot.value; // Clear NXE bit
    }

    return (pgframe);
}
static inline phys_addr_t bootmem_map_p2(p3e_t *p3e, pgprot_t pg_prot, vm_flags_t flags)
{
    phys_addr_t pgframe = 0;

    /* Se o pagetable já está mapeado, devolve o seu endereço físico. */
    if (p2_present(p3e))
    {
        pgframe = MASK_PAGE_ENTRY((*p3e).p3e);
    }
    /* Aloca um novo pageframe e insere seu endereço físico no entry. */
    else
    {
        pgframe = alloc_bootmem_pagetable(flags);
        clear_frame(phys_to_virt(pgframe));
        (*p3e).p3e = pgframe | pg_prot.value;
    }
    return (pgframe);
}
static inline phys_addr_t bootmem_map_p1(p2e_t *p2e, pgprot_t pg_prot, vm_flags_t flags)
{
    phys_addr_t pgframe = 0;

    /* Se o pagetable já está mapeado, devolve o seu endereço físico. */
    if (p1_present(p2e))
    {
        pgframe = MASK_PAGE_ENTRY((*p2e).p2e);
    }
    /* Aloca um novo pageframe e insere seu endereço físico no entry. */
    else
    {
        pgframe = alloc_bootmem_pagetable(flags);
        clear_frame(phys_to_virt(pgframe));
        (*p2e).p2e = pgframe | pg_prot.value;
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
static inline phys_addr_t bootmem_map_frame(p1e_t *p1e, phys_addr_t phys_addr, u64_t pg_flags)
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
static p1e_t *bootmem_mmap_frame(vm_area_t *vmm, virt_addr_t vaddr)
{
    virt_addr_t pml4 = init_mm.pml4;
    //*****************************
    p4e_t *p4e = p4_entry(pml4, (u64_t)vaddr);
    bootmem_map_p3(p4e, vmm->pgprot, vmm->flags);

    /* Mapeia um pagetable2 para a entrada no P3. */
    p3e_t *p3e = p3_entry(p4e, (u64_t)vaddr);
    bootmem_map_p2(p3e, vmm->pgprot, vmm->flags);

    /* Mapeia um pagetable1 para a entrada no P2. */
    p2e_t *p2e = p2_entry(p3e, (u64_t)vaddr);
    bootmem_map_p1(p2e, vmm->pgprot, vmm->flags);

    /* Mapeio o pageframe. */
    p1e_t *p1e = p1_entry(p2e, (u64_t)vaddr);

    return p1e;
}
/* Esta rotina só deve ser utilizada para mapear grandes e contínuas áreas
físicas/virtuais. O flag informará de que área estamos tratando e podemos
escolher o endereço físico inicial.*/

static void bootmem_loop_mmap_ranger(vm_area_t *vmm)
{
    virt_addr_t ini = vmm->vm_start;
    virt_addr_t pend = vmm->vm_pend;
    phys_addr_t pageframe = 0;

    if (vmm->flags & VM_IDENTI)
    {
        pageframe = (u64_t)ini;
    }
    else if (vmm->flags & VM_RANGER)
    {
        pageframe = vmm->phys_base;
    }
    else if (vmm->flags & VM_FALLOC)
    {
        kprintf("\nBOOTMEM - Não permitido o mapeamento de physic ranger descontinuo. ");
        debug_pause("ERRO - flag VM_FALLOC não permitida.");
    }

    kprintf("\nini=%p ->| pend=%p - pageframe=%x", ini, pend, pageframe);

    for (virt_addr_t i = ini; i < pend; i = incptr(i, PAGE_SIZE))
    {
        if (vmm->flags & VM_SYSTEM)
        {
            if (pageframe >= ((u64_t)&_rodata_begin_phys) &&
                pageframe < ((u64_t)&_rodata_end_phys))
            {
                // Somente leitura - limpa bit (Write)
                // Nao permite execução - seta bit 63
                // prot_tmp = (prot_tmp ^ PG_FLAG_NE);
                // debug_pause("VM_KERNEL");
            }
            // TEXT = Código executável
            else if (pageframe >= ((u64_t)&_text_begin_phys) &&
                     pageframe < ((u64_t)&_text_end_phys))
            {
                // Somente leitura - limpa bit (Write)
                // Permite execução - limpa bit 63
                // prot_tmp = 0;
            }
            else
            {
                // Permite leitura e gravação
                // prot_tmp = PG_FLAG_W;
            }

            if (pageframe > kernel_ini() && pageframe < kernel_pend())
            {
                /* PageTable statistic. */
                // mm_tables_alloc_counter_inc(&init_mm.mm_pgtables, PFRAME_ID);
            }
        }

        /*
        Esta rotina é executada para todos os frames do intervalo
        de memória.
        */
        p1e_t *p1e = bootmem_mmap_frame(vmm, i);

        if ((p1e->p1e) & PG_FLAG_P)
        {
            kprintf("\ni=%p ->| pend=%p - pageframe=%x", i, pend, pageframe);
            kprintf("\np1e=%p -> p1e->p1e=%x", p1e, p1e->p1e);

            DBG_LINE();
            pause_enter();
        }
        /**
         * @brief Faz o mapeamento do frame, que tem o seu endereço
         * físico inserido no entry do PageTable1.
         */

        // map_frame(p1e, pageframe, vmm->pgprot.value);
        bootmem_map_frame(p1e, pageframe, vmm->pgprot.value);
        pageframe = pageframe + PAGE_SIZE;
    }
}
static void mmap_bootmem(vm_area_t *vmm_area)
{
    kprintf("\n(*)%s(%d) - kvm_area_ini: Mapeando area virtual.", __FUNCTION__, __LINE__);

    virt_addr_t pml4 = init_mm.pml4;

    if (pml4 == NULL)
    {
        kprintf("\nLinha: %d - ERROR - VMM_AREA: init_mm.pml4==NULL.", __LINE__);
        debug_pause("\nErro fatal.");
    }

    bootmem_loop_mmap_ranger(vmm_area);

    if (__read_cr3() == virt_to_phys(pml4))
    {
        flush_tlb_range((phys_addr_t)vmm_area->vm_start, (phys_addr_t)vmm_area->vm_pend);
        return;
    }
    kprintf("\nvmm_area->vm_start=%p ->| vmm_area->vm_pend=%p", vmm_area->vm_start, vmm_area->vm_pend);

    kprintf("\n(*)%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}

static void bootmem_loop_unmmap_ranger(vm_area_t *vmm)
{
    virt_addr_t ini = vmm->vm_start;
    virt_addr_t pend = vmm->vm_pend;
    phys_addr_t pageframe = 0;
    virt_addr_t pml4 = init_mm.pml4;

    kprintf("\nini=%p ->| pend=%p - pageframe=%x", ini, pend, pageframe);

    for (virt_addr_t i = ini; i < pend; i = incptr(i, PAGE_SIZE))
    {
        p1e_t *p1e = get_pagetable_entry_p1e(pml4, i);

        if ((p1e->p1e) & PG_FLAG_P)
        {
            pageframe = MASK_PAGE_ENTRY((*p1e).p1e);

            // kprintf("\ni=%p ->| pend=%p - pageframe=%x", i, pend, pageframe);
            // kprintf("\np1e=%p -> p1e->p1e=%x", p1e, p1e->p1e);
            //  debug_pause("P1");
        }
    }
}
static void unmmap_bootmem(vm_area_t *vmm_area)
{
    kprintf("\n(*)%s(%d): Removendo mapeando temporário.", __FUNCTION__, __LINE__);

    virt_addr_t pml4 = init_mm.pml4;

    if (pml4 == NULL)
    {
        kprintf("\nLinha: %d - ERROR - VMM_AREA: init_mm.pml4==NULL.", __LINE__);
        debug_pause("\nErro fatal.");
    }

    bootmem_loop_unmmap_ranger(vmm_area);
    kprintf("\nvmm_area->vm_start=%p ->| vmm_area->vm_pend=%p", vmm_area->vm_start, vmm_area->vm_pend);

    kprintf("\n(*)%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}