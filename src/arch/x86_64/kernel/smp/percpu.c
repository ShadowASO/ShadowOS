/*--------------------------------------------------------------------------
*  File name:  percpu.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 02-04-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a tabela
IDT e seus descritores, necessários para a manipulação das interrupções.
--------------------------------------------------------------------------*/
#include "ktypes.h"
#include "smp.h"
#include "percpu.h"
#include "x86_64.h"
#include "mm/kmalloc.h"
#include "lapic.h"
#include "gdt.h"
#include "msr.h"
#include "mm/fixmap.h"
#include "tss.h"
#include "gdt.h"
#include "idt.h"
#include "interrupt.h"

/*----------------------------------------*/
/* Criamos três vetores cujos elementos são a GDT, IDT e TSS que será utilizada por cada CORE.
Antes estávamos criando as tabelas dinamicamente, mas essa forma estática parece mais es-
tável e segura. */
// tss_t tss_percpu[MAX_CORES] __section(".data");
__attribute__((aligned(0x1000))) tss_t tss_percpu[MAX_CORES] = {0};
__attribute__((aligned(0x1000))) gdt_t gdt_percpu[MAX_CORES] = {0};
__attribute__((aligned(0x1000))) idt_t idt_percpu[MAX_CORES] = {0};

struct percpu percpu_table[MAX_CORES] = {0};

tss_t *percepu_get_tss(u8_t cpu)
{
	tss_t *tss = &tss_percpu[cpu];
	return tss;
}

/* Extrai o endereço do percpu no GS através do MSR. */
struct percpu *percpu_get(void)
{
	return (virt_addr_t)msr_read(MSR_GSBASE);
}
/* Grava o endereço do percpu no GS através do MSR. */
void percpu_set(struct percpu *p)
{
	msr_write(MSR_GSBASE, (u64_t)p);
}
/*
Seleciona uma nova IDT do vetor idt_percpu, faz a limpeza e copia para
ela o conteúdo da tabela idt_table. Em seguida, carrega o respectivo re-
gistro. A rotina __load_idt() está preparada para construir o ponteiro,
recebendo o endereço da IDT e o seu tamanho em bytes.
Sem uma IDT para cada CORE, estava ocorrendo erros intermitentes, na ini-
cialização.
*/
void percpu_load_idt(void)
{
	u8_t id_cpu = cpu_id();
	idtr_t pidt_r = {0};

	virt_addr_t idt = &idt_percpu[id_cpu];
	memset(idt, 0, sizeof(idt_t));
	memcpy(idt, idt_table, sizeof(idt_t));

	/* Carrego IDT.  */
	__load_idt((mm_addr_t)idt, (sizeof(idt_t) - 1));
}
/* Executada para cada AP, faz a carga das tabelas IDT, TSS e GDT. */

void percpu_init_ap(void)
{
	u8_t id_cpu = cpu_id();
	struct percpu *cpu = &percpu_table[id_cpu];

	/*	IDT -	Carrego IDT. */
	percpu_load_idt();
	/*------------------------*/

	/*
	Utilizo o número do CORE como um índice dentro do vetor gdt_percpu
	para encontrar a respectiva GDT, limpo e copia o conteúdo de gdt64.
	*/
	virt_addr_t gdt = &gdt_percpu[id_cpu];
	memset(gdt, 0, sizeof(gdt_t));
	memcpy(gdt, gdt64, sizeof(gdt_t));
	/*----------------------------------------*/

	/* Utilizo o número do CORE como um índice dentro do vetor tss_percpu[]
	para encontrar a respectiva TSS e a limpa. */
	tss_t *tss = &tss_percpu[id_cpu];
	memset(tss, 0, sizeof(tss_t));
	/* Crio uma stack para o IST* indicado, a partir do FIXMAP.*/
	tss->ist1 = (mm_addr_t)get_ist_stack(cpu_id(), TSS_IST_POS1); // NMI

	/*---------------------------------------------*/

	/* Vinculo a nova TSS à GDT.*/
	create_system_descriptor(gdt, _GDT_TSS_SEL, (mm_addr_t)tss, sizeof(tss_t) - 1, SYSTEM_TYPE_TSS, SYSTEM_FLAGS_G_BYTE);

	/* Carrego a nova tabela GDT e faço um far jump para serializar CS/GDT/KERNEL_CODE.
	Sem isso o sistema fica instável e produz erros intermitente na inicialização.*/

	__load_gdt((u64_t)gdt, (sizeof(gdt_t) - 1), _GDT_KERNEL_CODE_SEL);

	/* Carrego a nova tabela TSS. */
	__load_tss(_GDT_TSS_SEL);

	/* Configuro e carrego a estrutura percpu.*/
	cpu->cpu_id = id_cpu;
	cpu->current_task = NULL;
	cpu->rsp_scratch = NULL;
	cpu->preempt_count = 0;
	cpu->tss = tss;

	percpu_set_addr(cpu);
}

/* Esta rotina faz a atribuição da primeira estrutura PERCPU para o núcleo BSP. */
void percpu_init_bsp(void)
{
	u8_t id_cpu = cpu_id();
	struct percpu *cpu = &percpu_table[id_cpu];

	/*	IDT -	Carrego logo a IDT. */
	virt_addr_t idt = &idt_percpu[id_cpu];
	__load_idt((mm_addr_t)idt, (sizeof(idt_t) - 1));

	/* GDT  */
	virt_addr_t gdt = &gdt_percpu[id_cpu];
	memset(gdt, 0, sizeof(gdt_t));
	memcpy(gdt, gdt64, sizeof(gdt_t));

	/* TSS */
	tss_t *tss = &tss_percpu[id_cpu];
	memset(tss, 0, sizeof(tss_t));
	// memcpy(tss, tss64, sizeof(tss_t));
	tss->ist1 = ((tss_t *)tss64)->ist1;
	tss->ist2 = ((tss_t *)tss64)->ist2;
	tss->ist3 = ((tss_t *)tss64)->ist3;

	/* Vinculo a nova TSS à GDT.*/
	create_system_descriptor(gdt, _GDT_TSS_SEL, (mm_addr_t)tss, sizeof(tss_t) - 1, SYSTEM_TYPE_TSS, SYSTEM_FLAGS_G_BYTE);

	/* Carrego a nova tabela GDT e faço um far jump para serializar CS/GDT/KERNEL_CODE.
	Sem isso o sistema fica instável e produz erros intermitente na inicialização.*/
	__load_gdt((u64_t)gdt, (sizeof(gdt_t) - 1), _GDT_KERNEL_CODE_SEL);

	/* ATENÇÃO: NÃO Carregar a nova tabela "TSS", pois isso  será feito no "entry.s->long_mode_entry()".
	O TSS não pode ser carregado antes das tabelas de interrupção - IDT, pois
	gera um erro logo na inicialização. */

	/* Atualizo o percpu e gravo o endreço da estrutura no GSbase do CORE. */
	cpu->tss = tss;
	percpu_set_addr(cpu);
}
/* Como cada núcleo possui um conjunto próprio de registros(RAX, GS, FS) eles podem ser utilizados
 independentemente. Neste caso, o registro GS está sendo utilizado para guardar o endereço da
 estrutura PERCPU, que contem dados da current task naquele processador. */

void percpu_set_addr(struct percpu *p)
{
	msr_write(MSR_GSBASE, (u64_t)p);
}

virt_addr_t get_ist_stack(u8_t core, u8_t ist)
{
	virt_addr_t vist = kfixmap(FIX_TSS_IST - (core * 3 + ist - 1));

	phys_addr_t stack_top = page_to_phys(alloc_page(zone_normal_id()));

	kmap_frame(vist, stack_top, pgprot_PW);
	memset(vist, 0, (PAGE_SIZE));

	return (virt_addr_t)incptr(vist, PAGE_SIZE - 16);
}
