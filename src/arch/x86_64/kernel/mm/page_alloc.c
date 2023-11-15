/*--------------------------------------------------------------------------
*  File name:  frame_alloc.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 02-05-2022
*--------------------------------------------------------------------------
Este heap reune estruturas para o Page Memory Allocator
--------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include "stdlib.h"
#include "stdio.h"
#include "debug.h"
#include "string.h"
#include "list.h"
#include "mm/page_alloc.h"
#include "mm/page.h"
#include "mm/mm_types.h"

/* Faz a inicialização das free_lists de cada uma das zonas.*/
void setup_zone_free_list(void)
{
    zone_t *zone;
    free_list_head_t *free_lists = NULL;

    kprintf("\n(*)%s(%d) - INIT FREE LISTS BY ZONE: Inicializando as free lists por zona.", __FUNCTION__, __LINE__);

    for (u8_t i = 0; i < MAX_ZONE_MEMORY; i++)
    {
        zone = zone_obj(i);
        zone->zone_free_pages.value = 0;
        free_lists = (free_list_head_t *)&zone->free_lists;

        for (u8_t x = 0; x <= MAX_PAGE_ORDER; x++)
        {
            // INIT_LIST_HEAD(&free_lists[x].list);
            init_list_head(&free_lists[x].list);
        }

        if (i == ZONE_DMA)
        {
            zone->zone_page_map = get_virt_memmap_offset();
            zone->zone_start_pfn = zone_dma_start_pfn();
            zone->flags = ZONE_DMA;
        }
        else if (i == ZONE_NORMAL)
        {
            zone->zone_page_map = get_virt_memmap_offset();
            zone->zone_start_pfn = zone_normal_start_pfn();
            zone->flags = ZONE_NORMAL;
        }
    }

    kprintf("\n(*):%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}

static inline unsigned int buddy_order(struct page *page)
{
    /* PageBuddy() must be checked by the caller */
    return page_order(page);
}

/**
 * Verifica se o bloco é um buddy válido e pode ser usado para
 * coalescência.
 * a) verifica se o bloco está na free list(is buddy);
 * b) verifica se o bloco está na mesma ordem;
 * c) verifica se o bloco está na mesma zona.
 *
 * For recording whether a page is in the buddy system, we set PageBuddy.
 * Setting, clearing, and testing PageBuddy is serialized by zone->lock.
 *
 * For recording page's order, we use page_private(page).
 */
static inline bool is_page_buddy_ok(struct page *bck, struct page *bud,
                                    unsigned int order)
{
    if (!page_buddy(bud))
    {
        return false;
    }

    if (buddy_order(bud) != order)
        return false;
    /*
     * zone check is done late to avoid uselessly calculating
     * zone/node ids for pages that could never merge.
     */
    if (page_zone_id(bck) != page_zone_id(bud))
        return false;

    return true;
}

/** Rotina que localizar o bloco parceiro(buddy) do bloco indicado, na ordem
 * considerada
 * return page_pfn ^ (1 << order);
 * */

static inline u64_t buddy(u64_t bck, int64_t order)
{
    u64_t mask = (1 << (order));
    bck = bck ^ mask;
    return bck;
}

/**
 * @brief Esta rotina foi generalizada para receber o início de um bloco
 * numérico cuja QUANTIDADE(número de itens) total seja um múltiplo de 2
 *
 * @param ini_bck
 * @param order
 * @return u64_t
 */
static inline u64_t split(u64_t bck, int64_t order)
{
    u64_t mask = (1 << (order - 1));
    bck = bck | mask;
    return bck;
};

/**
 * Devolve o buddy que compõe a parte inferior do bloco a ser colacionado.
 */

static inline u64_t primary(u64_t bck, int64_t order)
{
    u64_t mask = (1 << (order));
    bck = (bck | mask) ^ mask;
    return bck;
};

/**
 * @brief Funções que emcapsulam as rotinas utilitárias split, buddy, primary
 *
 */
static inline page_t *get_buddy(page_t *bck)
{
    u64_t pfn = page_to_pfn(bck);
    pfn = buddy(pfn, bck->level);
    return (page_t *)pfn_to_page(pfn);
}
static inline page_t *get_primary(page_t *bck)
{
    u64_t pfn = page_to_pfn(bck);
    pfn = primary(pfn, bck->level);
    return (page_t *)pfn_to_page(pfn);
}
static inline page_t *get_split(page_t *bck)
{
    u64_t pfn = page_to_pfn(bck);
    pfn = split(pfn, bck->level);
    return (page_t *)pfn_to_page(pfn);
}

static inline page_t *get_free_block(free_list_head_t *free_lists, u8_t order)
{
    if (!is_list_empty(free_lists, order))
    {
        return container_of(free_lists[order].list.next, page_t, node);
    }
    return NULL;
}

/* Adiciona um novo bloco à free list. */
static inline void block_add(free_list_head_t *free_lists, page_t *bck)
{
    free_list_head_t *head = &free_lists[bck->level];

    list_add(&bck->node, &head->list);
    set_page_buddy(bck);

    free_list_head_counter_inc(head);
}
/* Deleta um bloco da free list. */
static inline void block_del(free_list_head_t *free_lists, page_t *bck)
{
    free_list_head_t *head = &free_lists[bck->level];

    list_del(&bck->node);
    clear_page_buddy(bck);
    free_list_head_counter_dec(head);
}

/**
 * Esta rotina faz a busca nas listas de blocos livre, utilizando 'order' para
 * localizar a lista dentro do vetor de listas. Como o tamanho da memória é encontrado
 * com a fórmula (2^order/1<<order), isso é tudo que precisamos. Caso não seja encontrado um bloco
 * com o tamanho desejado na respectiva lista, subimos para a lista superior, até
 * achar um bloco livre. Depois fazemos o caminho inverso, quebrando o bloco sucessiva-
 * mente, até chegarmos a um bloco do tamanho desejado. A regra é sempre utilizar a metade
 * inferior de cada bloco quebrado.
 */
static page_t *__buddy_find_block(zone_t *zone, u8_t order)
{
    page_t *bck = NULL;
    page_t *sup = NULL;

    free_list_head_t *free_lists = zone_freelist(zone);

    /* Buscamos nas filas acima, até encontrar um bloco livre. */
    for (u8_t i = order; i <= MAX_PAGE_ORDER; i++)
    {
        if (!is_list_empty(free_lists, i))
        {
            bck = get_free_block(free_lists, i);
            goto block_found;
        }
    }

    /* Se nenhum bloco for encontrado, devolvemos um NULL. Não existem blocos com o tamanho
    desejado ou superior. O usuário precisa testar o retorno. */
    return NULL;

    /* Se foi encontrado um bloco nas filas superiores, ele é de tamanho superior ao buscado.
    Fazemos um loop quebrando esse bloco em blocos menores, até chegar ao tamanho(order) de-
    sejado. */

block_found:
    block_del(free_lists, bck);

    while (bck->level > order)
    {
        sup = get_split(bck);
        bck->level--;

        /* O bloco é dividido em duas partes(buddy's) iguais, devolvendo a metade su-
        perior do bloco. Ajusto os novos blocos(level e status) e os insiro na fila ime-
        diatamente inferior. */

        sup->level = bck->level;
        block_add(free_lists, sup);
    }

    return bck;
}
/**
 * Quando há a liberação de um bloco de memória anteriormente alocado, ele deve ser
 * inserido em uma das listas de blocos livres, de acordo com o seu tamanho em bytes.
 * Caso o buddy(o par) do bloco esteja livre, ambos são mesclados e inseridos na
 * lista de blocos superior, cujos blocos tem o tamanho dobrado(soma dos buddy's)
 */

static void __buddy_free_block(zone_t *zone, page_t *bck)
{
    page_t *bud = NULL;
    page_t *ini = NULL;

    free_list_head_t *free_lists = zone_freelist(zone);

    /* Inserimos o bloco imediatamente.*/
    block_add(free_lists, bck);

    /*Verifico se o o bloco  inserido já alcança o limite das filas. Se já está
    no limite, simplesmente retornamos. Caso contrário, continuamos para tentar
    fazer a coalescência. */

    if (bck->level == MAX_PAGE_ORDER)
    {
        return;
    }

    /* Pegamos o bloco buddy(o par) para testes que se seguem. */
    bud = get_buddy(bck);

    if (!is_page_buddy_ok(bck, bud, bck->level))
    {
        return;
    }

    /* Se chegamos neste ponto, significa que o bloco não alcançou o tamanho máximo
    e que o seu buddy também está livre, e pode haver a coalescência.*/

    /*-----------------------------------------------------------------------------
    A partir deste ponto, "ini" passa a ser tratado como a parte inferior "bud"  a
    parte superior de uma coalescência. */
    ini = get_primary(bck);
    bud = get_buddy(ini);
    /*-----------------------------------------------------------------------------*/

    do
    {
        /* Como os blocos buddy estão livre, haverá a coalescência. Fazemos a
        exclusão de ambos. */

        block_del(free_lists, bud);
        block_del(free_lists, ini);

        /* A coalescência importa na inserção do dois bloco como um único bloco
        na fila de ordem imediatamene superior. Isso é indicado no level     do
        bloco inicial do par de buddys. */

        ini = get_primary(ini);
        ini->level++;

        /* Insiro os dois buddys na fila imediatamente acima.*/

        block_add(free_lists, ini);

        /* Como o bloco "ini" acabou de ser inserido na fila, descubro o seu
         bloco buddy na nova fila. Se ele não estiver livre, encerro a coalescência e
         retorno. */

        bud = get_buddy(ini);

        if (!is_page_buddy_ok(ini, bud, ini->level))
        {
            return;
        }

        /* Se os dois blocos buddy estão livres, reiniciamos o ciclo, fazendo a
        exclusão de ambos e inserindo o primeiro na fila imediatamente acima,
        salvo se já estamos no MAX_PAGE_ORDER. */

    } while (ini->level < MAX_PAGE_ORDER);
}

/**
 * Rotina para o usuário final requisitar memória
 */

static page_t *buddy_alloc_pages(gfp_t gfp_zone, size_t order)
{
    zone_t *zone = zone_obj(gfp_zone);

    if (order >= MAX_PAGE_ORDER)
    {
        WARN_ON("order não pode ser superior a %d.", MAX_PAGE_ORDER);
        return NULL;
    }

    if (zone->zone_page_map == NULL)
    {
        kprintf("kalloc: Inicialize kalloc - [init_kalloc(void)]");
    }

    page_t *taken = __buddy_find_block(zone, order);

    if (taken == NULL)
        return NULL;

    /* Atualiza o número de free pages no node e zone. */
    update_free_pages_sub(zone, order);
    /*---------------------------------------------------*/

    return taken;
}
/**
 * Rotina para devolver a memória não mais utilizada pelo usuário
 */
static int buddy_free_page(page_t *page)
{
    if (page == NULL)
    {
        return -1;
    }
    int idx = page_zone_id(page);
    zone_t *zone = page_zone(page);

    /* Atualiza o número de free pages no node e zone. */
    update_free_pages_add(zone, page->level);
    /*-------------------------------------------------*/

    __buddy_free_block(zone, page);
    set_page_free(page);

    return 0;
}

/**
 * @brief Recebe um bloco e verifica se cada uma das pages não
 * está sendo utilizada, fazendo a inserção por pages na lista
 * de pages free.
 *
 * @param bck
 * @param order
 */
static inline void free_block_uncontinuos(page_t *bck, int order)
{
    size_t nr_pages = (1 << order);
    size_t pfn = page_to_pfn(bck);
    page_t *page;

    for (size_t i = 0; i < nr_pages; i++)
    {
        page = pfn_to_page(i + pfn);
        if (!page_is_fixmap(page) && !page_is_free(page))
        {
            free_pages(page);
            set_page_buddy(page);
        }
    }
}
static inline void prepare_pages_block(page_t *bck, int order)
{
    size_t nr_pages = (1 << order);
    page_t *page = NULL;

    set_page_head(bck);
    set_page_used(bck);
    /* Configuro a tail pages. */
    for (size_t i = 1; i < nr_pages; i++)
    {
        page = bck + i;
        clear_page_head(page);
        set_block_level(page, 0);
        set_page_used(page);
    }
}

/**
 * @brief Verifica se um bloco de pages é composto apenas por pages
 * não utilizados.
 *
 * @param page
 * @param order
 * @return true
 * @return false
 */
static bool is_pages_block_ok(page_t *bck, int order)
{
    size_t nr_pages = (1 << order);
    page_t *page;

    for (size_t i = 0; i < nr_pages; i++)
    {
        page = bck + i;
        if (page_is_fixmap(page) || !page_is_free(page))
        {
            return false;
        }
    }
    return true;
}

static page_t *__alloc_pages(gfp_t gfp_mask, size_t order)
{
    page_t *bck = buddy_alloc_pages(gfp_mask, order);
    if (order == 0)
    {
        // return bck;
        // set_page_used(bck);
    }
    /* Se o bloco for maior que um PAGE_SIZE, Verifico se o o bloco encontrado é consistente.
    Caso contrário, faço uma chamada recursiva até encontrar um bloco consistente. Aqui, con-
    sistente é aquele bloco em que todos os pages são free. Há o risco de estouro da pilha.
    Ficar atendo.
    */

    WARN_DEBUGING("Entrando is_pages_block_continuos.");
    if (!is_pages_block_ok(bck, order))
    {
        free_pages(bck);
        bck = alloc_pages(gfp_mask, order);
    }
    /* Ajusta os atributos de todos os pages que compõem o bloco.*/
    prepare_pages_block(bck, order);

    return bck;
}
page_t *alloc_pages(gfp_t gfp_mask, size_t order)
{
    return __alloc_pages(gfp_mask, order);
}

int free_pages(page_t *page)
{
    return buddy_free_page(page);
}
