/*--------------------------------------------------------------------------
*  File name:  kalloc.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 31-08-2020
*  Revisão 05/08/2022.
*--------------------------------------------------------------------------
Este header reune estruturas para o Page Memory Allocator
--------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include "stdlib.h"
#include "mm/kmalloc.h"
#include "stdio.h"
#include "debug.h"
#include "string.h"
#include "list.h"
#include "mm/mm_heap.h"

/**
 * @brief Faço a inicialização das listas para serem utilizada pelo
 * algoritmo do buddy
 *
 */
void init_buddy_free_lists()
{
    kprintf("\nLinha: %d - INIT_KMALLOC: Inicializando listas de kmalloc.", __LINE__);

    for (int i = 0; i <= MAX_HEAP_ORDER; i++)
    {
        // INIT_LIST_HEAD(&kmm_heap.free_lists[i].list);
        init_list_head(&kmm_heap.free_lists[i].list);
        kmm_heap.free_lists[i]
            .counter.value = 0;
    }

    kprintf("\nLinha: %d - INIT_KMALLOC: Finalizado.", __LINE__);
}

/**
 * Rotina utilitária que localiza o bloco parceiro(buddy) do bloco indicado
 */
static inline u64_t buddy(u64_t bck, int64_t order)
{
    u64_t mask = (1 << (order));
    bck = bck ^ mask;
    return bck;
}

/**
 * Dado um block, a rotina devolve a metade superior
 */
static inline u64_t split(u64_t bck, int64_t order)
{
    u64_t mask = (1 << (order - 1));
    bck = bck | mask;
    return bck;
};

/**
 * Devolve a block inferior de uma dupla buddys
 */
static inline u64_t primary(u64_t bck, int64_t order)
{
    u64_t mask = (1 << (order));
    bck = (bck | mask) ^ mask;
    return bck;
};

static freeHead_t *get_buddy(freeHead_t *bck)
{
    u64_t head = (u64_t)bck;
    head = buddy(head, bck->level);
    return (freeHead_t *)(head);
}
/* Devolve a metade superior do bloco. */
static freeHead_t *get_split(freeHead_t *bck)
{
    u64_t head = (u64_t)bck;
    head = split(head, bck->level);
    return (freeHead_t *)(head);
}
static freeHead_t *get_primary(freeHead_t *bck)
{
    u64_t head = (u64_t)bck;
    head = primary(head, bck->level);
    return (freeHead_t *)(head);
}

/**
 * Devolve o endereço incial do espaço disponível de um bloco,
 * excluindo o header
 */
static inline void *hide(usedHead_t *block)
{
    return (void *)(block + 1); // Avaça o correspondente a sizeof(usedHeader_t)
};
/**
 * Acrescenta o header e devolve o bloco.
 */
static usedHead_t *magic(void *memory)
{
    return ((usedHead_t *)memory - 1); // Recua o correspondente a sizeof(usedHeader_t)
};
/**
 * @brief Recebe um tamanho de memória requisitada e acrescenta o
 * tamanho do seu cabeçalho.
 *
 * @param bck_size
 * @return int
 */
static inline u32_t block_size(int bck_size)
{
    // Capturo o menor tamanho de bloco passível de alocação. O head torna impossível
    // tamanhos menores que ele.
    size_t min_size = sizeof(freeHead_t);

    // Calculo o tamanho de bloco adicionando o header menor utilizado para os blocos
    // alocados.

    size_t total = bck_size + sizeof(usedHead_t);

    // Se o bloco tiver um tamanho inferior ao head de um bloco livre, este será o
    // seu tamanho mínimo.
    total = (total >= min_size) ? total : min_size;

    return total;
}

/**
 * Calcular o level com tamanho adequado para comportar a memória
 * requisitada, adicionando o tamanho do header
 */
static inline u8_t level(int mm_size)
{
    /* Capturo o menor tamanho de bloco passível de alocação. O head torna impossível
    tamanhos menores que ele. */
    size_t min_size = sizeof(freeHead_t);

    // Calculo o tamanho de bloco adicionando o header menor utilizado para os blocos
    // alocados.

    size_t total = mm_size + sizeof(usedHead_t);

    // Se o bloco tiver um tamanho inferior ao head de um bloco livre, este será o
    // seu tamanho mínimo.
    total = (total >= min_size) ? total : min_size;
    size_t i = calc_bin_order(total);

    return i;
}
/**
 * @brief Testa se o buddy está dentro do espaço de memória
 * alocado para o heap. Do contrário, corremos o risco de ter
 * um buddy iniciando além da memória alocada, provocando um
 * paga fault
 *
 * @param bck
 * @return true
 * @return false
 */

static inline bool is_buddy_free(freeHead_t *bud)
{

    if ((virt_addr_t)bud >= heap_pend())
    {
        return false;
    }
    if (bud->status != eBUDDY_FREE)
    {
        return false;
    }

    return true;
}

static inline freeHead_t *get_free_block(u8_t order)
{
    free_list_head_t *free_lists = kmm_heap.free_lists;

    if (!is_list_empty(free_lists, order))
    {
        return container_of(kmm_heap.free_lists[order].list.next, freeHead_t, node);
    }
    return NULL;
}
/* Adiciona um novo bloco à free list. */
static inline void block_add(freeHead_t *bck)
{
    free_list_head_t *head = &kmm_heap.free_lists[bck->level];

    list_add(&bck->node, &head->list);
    bck->status = eBUDDY_FREE;

    free_list_head_counter_inc(head);
}
/* Deleta um bloco da free list. */
static inline void block_del(freeHead_t *bck)
{
    free_list_head_t *head = &kmm_heap.free_lists[bck->level];

    list_del(&bck->node);
    bck->status = eBUDDY_TAKE;

    free_list_head_counter_dec(head);
}
/**
 * Esta rotina faz a busca nas listas de blocos livre, utilizando o valor index para
 * localizar a lista dentro do vetor de listas. Como o tamanho da memória é encontrado
 * com a fórmula (2^index), isso é tudo que precisamos. Caso não seja encontrado um bloco
 * com o tamanho desejado na respectiva lista, subimos para a lista de blocos maiores, até
 * achar um bloco livre. Depois fazemos o caminho inverso, quebrando o bloco sucessiva-
 * mente, até chegarmos a um bloco do tamanho desejado.
 */
static freeHead_t *find(int order)
{
    freeHead_t *bck = NULL;
    freeHead_t *sup = NULL;
    free_list_head_t *free_lists = kmm_heap.free_lists;

    /* Verificamos se existem blocos livre nas filas, sempre buscamos nas fila
    acima, até encontrar. */

    for (int i = order; i <= MAX_HEAP_ORDER; i++)
    {
        if (!is_list_empty(free_lists, i))
        {
            bck = get_free_block(i);
            goto block_found;
        }
    }
    /* Se nenhum bloco for encontrado, devolvemos um NULL. Usuário precisa testar o
    retorno. */
    return NULL;

    /* Se foi encontrado um bloco nas filas superiores, ele é de tamanho superior ao buscado.
    Fazemos um loop quebrando esse bloco em blocos menores, até chegar ao tamanho(order) de-
    sejado. */

block_found:

    block_del(bck);

    while (bck->level > order)
    {
        sup = get_split(bck);
        bck->level--;

        /* O bloco é divido em duas partes(buddy's) iguais, devolvendo a metade su-
        perior do bloco. Ajusto os atributos da metade superior do bloco(level e status)
        e o insiro na fila imediatamente inferior. */

        sup->level = bck->level;
        block_add(sup);
    }
    return bck;
}

/**
 * Quando há a liberação de um bloco de memória anteriormente alocado, ele deve ser
 * inserido em uma das listas de blocos livres, de acordo com o seu tamanho em bytes.
 * Caso o buddy(o par) do bloco esteja livre, ambos são mesclados e inseridos na
 * lista de blocos superior, cujos blocos tem o tamanho dobrado(soma dos buddy's)
 */
static void insert(freeHead_t *bck)
{
    freeHead_t *bud = NULL;
    freeHead_t *ini = NULL;

    /*Insere bloco na respectiva fila e altera o seu status. */
    block_add(bck);

    /*Verifico se o o bloco  inserido já alcança o limite das filas. Se já está
    no limite, simplesmente retornamos. Caso contrário, continuamos para tentar
    fazer a coalescência. */

    if (bck->level == MAX_HEAP_ORDER)
    {
        return;
    }

    /* Pegamos o bloco buddy(o par) para verificar se ele também está livre. Se
    negativo, retornamos, pois não poderá haver coalescência. Se afirmativo, se-
    guimos em frente. */
    bud = get_buddy(bck);

    if (!is_buddy_free(bud))
    {
        return;
    }

    /* Se chegamos neste ponto, significa que o bloco buddy também está livre,
    e pode haver a coalescência.*/

    /*-----------------------------------------------------------------------------
    A partir deste ponto, "ini" passa a representar a parte inferior e "bud" a
    parte superior de um par de buddys em coalescência. */

    ini = get_primary(bck);
    bud = get_buddy(ini);
    /*-----------------------------------------------------------------------------*/

    /* Este loop percorrerá todos as filas acima, tentando realizar a coa-
    lescência. */

    // do
    while (ini->level < MAX_HEAP_ORDER)
    {

        /* Como os blocos buddy estão livre, pode haver a coalescência. Assim,
        ambos devem ser excluídos e reinseridos como um único bloco na fila ime-
        diatamente acima. */

        block_del(bud);
        block_del(ini);

        /* A coalescência importa na inserção do dois bloco como um único bloco
        na fila de ordem imediatamene superior. Isso é indicado no level     do
        bloco inicial do par de buddys. */

        ini = get_primary(ini); // Represente os dois blocos excluídos.
        ini->level++;

        /* Insiro o novo bloco na fila imediatamente acima. */
        block_add(ini);

        /* Como o bloco "ini" acabou de ser inserido na fila, descubro o seu
         bloco buddy na nova fila. Se ele estiver livre, reiniciamos todo o
         ciclo. Do contrário, encerra-se a coalescência e retorna. */

        bud = get_buddy(ini);
        // ini = get_primary(ini);

        /* Se o bloco buddy desse novo bloco estiver livre, fazemos uma nova
        coalescência, subindo para a fila logo acima. CAso contrário, simples-
        mente retornamos. */

        if (!is_buddy_free(bud))
        {
            // return;
            break;
        }

        /* Se os dois blocos buddy estão livres, reiniciamos o ciclo, fazendo a
        exclusão de ambos e inserindo o primeiro na fila imediatamente acima,
        salvo se já estamos no MAX_PAGE_ORDER. */

        //} while (ini->level < MAX_HEAP_ORDER);
    }
    return;
}

/**
 * Rotina para o usuário final requisitar memória
 */

virt_addr_t kmalloc(size_t mm_size)
{
    if (mm_size == 0)
    {
        WARN_ON("\nRequisitada memory igual a 0");
        return NULL;
    }

    /* Calculo o leve e o tamanho do bloco a ser utilizado.
    O level é calculado adicionando o sizeof(usedHead_t)*/

    int index = level(mm_size);
    u64_t bck_size = (1 << index);

    if (bck_size > (MAX_BLOCK_SIZE - sizeof(usedHead_t)))
    {
        WARN_ON("Memory requisitada %d maior que %d.\n", mm_size, MAX_BLOCK_SIZE - sizeof(usedHead_t));
        return NULL;
    }
    /* Se a memória livre na heap não for suficiente para atender à demanda, */
    if (heap_free_mm() < bck_size)
    {
        WARN_ON("\nAllocando Memory to Heap = %d.", heap_free_mm());

        /* Alloca mais memória para a heap. */
        brk(MIN_BLOCK_SIZE);
    }

    /* modifico o header do bloco de memória devolvido da freelist.
     Isso economiza memória. */

    usedHead_t *taken = (usedHead_t *)find(index);
    if (taken == NULL)
    {
        WARN_ON("\nkfree: taken=%p : mm_size=%d - bck_size=%d - index=%d", taken, mm_size, bck_size, index);
        return taken;
    }

    taken->level = index;
    taken->status = eBUDDY_TAKE;

    update_heap_free_dec(bck_size);

    /* Exclui o header do bloco e devolve o endereço de inicio do espaço útil.*/
    return hide(taken);
}
/**
 * Rotina para devolver a memória não mais utilizada pelo usuário
 */
void kfree(void *memory)
{
    if (memory != NULL)
    {
        /* Aplico o cabeçalho de blocos livres, sem preocupação com a eventual
        sobreposição dos dados, já que não tem mais utilidade.*/

        freeHead_t *block = (freeHead_t *)magic(memory);
        int index = block->level;
        u64_t bck_size = (1 << index);

        /*Insiro o bloco de memória nas listas livres. */
        insert(block);

        /*Incremento a memória livre o heap. */
        update_heap_free_inc(bck_size);
    }
    return;
}

void __brk(freeHead_t *bck)
{
    insert(bck);
}
