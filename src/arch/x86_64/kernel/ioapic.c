/*--------------------------------------------------------------------------
 *  File name:  ioapic.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 05-06-2021
 *  Revisão: 18-05-2022
 *--------------------------------------------------------------------------
 *  Rotina para configuração e uso do I/O APIC
 *  O IRQ gerada pelo PIT muda do IRQ0 para o IRQ2
 *--------------------------------------------------------------------------*/
#include "ctype.h"
#include "bits.h"
#include "lapic.h"
#include "stdio.h"
#include "io.h"
#include "mm/vmm.h"
#include "mm/page.h"
#include "debug.h"
#include "info.h"
#include "sync/mutex.h"
#include "ktypes.h"
#include "pic.h"
#include "mm/fixmap.h"
#include "interrupt.h"

// #define IOAPIC_BASE 0xFEC00000 // Default physical address of IO APIC

// IOAPIC Registers
#define IOAPIC_ID 0x00
#define IOAPIC_VER 0x01
#define IOAPIC_ARB 0x02

// Aponta para a metade inferior de cada registro de 64 bits da tabela de redireção

#define REDTBL_BASE 0x10
#define REDTBL_ENTRY(x) (((x) * 2) + REDTBL_BASE)

// Flags de configuração
#define IOAPIC_IRQ_MASK 0x00010000		// Interrupt disabled
#define IOAPIC_IRQ_LEVEL 0x00008000		// Level-triggered (vs edge-)
#define IOAPIC_IRQ_ACTIVELOW 0x00002000 // Active low (vs high)
#define IOAPIC_IRQ_LOGICAL 0x00000800	// Destination is CPU id (vs APIC ID)

#define GET_IO_APIC_VERSION(x) ((x) & 0xFF)
#define GET_IO_APIC_ENTRY(x) (((x) >> 16) & 0xFF) // max entry number

#define SET_IO_APIC_DEST(x) (((x) & 0xFF) << 56)
#define IO_APIC_IRQ_MASK (1 << 16)
#define IO_APIC_TRIG_MODE (1 << 15)
#define IO_APIC_IRR (1 << 14)
#define IO_APIC_INTPOL (1 << 13)
#define IO_APIC_DELIVS (1 << 12)
#define IO_APIC_DESTMOD (1 << 11)
#define IO_APIC_DELMOD_FIX ((0) << 8)
#define IO_APIC_DELMOD_LOW ((1) << 8)
#define IO_APIC_DELMOD_SMI ((2) << 8)
#define IO_APIC_DELMOD_NMI ((4) << 8)
#define IO_APIC_DELMOD_INIT ((5) << 8)
#define IO_APIC_DELMOD_ExtINT ((7) << 8)

#define attrib_packed __attribute__((packed))

/*
 * Esta estrutura espelha o mecanismo de leitura mmio utilizada
 * para escrever e ler registros do IO Apic base. Com ela é pos-
 * sível ler todos os registros.
 * Ex.IOAPIC_BASE_ADDRESS 0xFEC00000.
 * Ao apontarmos a estrutura abaixo para o endereço acima,
 * o field setReg==IOREGSEL. Se atribuirmos o offset de um dos
 * registros, será possível ler ou escrever o seu conteúdo por
 * meio do field getData==IOWIN.
 */
typedef struct mmio_ioapic
{
	uint32_t setReg;
	uint32_t padding[3];
	uint32_t getData;
} mmio_ioapic_t;
/**
 * @brief  Esta estrutura representa os registros de redireção
 * de interrupção, com seus diversos campos. Como possui 64 bits,
 * deve ser dividida no momento de escrevê-la de volta aos re-
 * gistro do I/O apic.
 * @note
 * @retval None
 */
typedef union
{
	struct redtbl
	{
		u64_t vector : 8;
		u64_t delvMode : 3;
		u64_t destMode : 1;
		u64_t delvStatus : 1;
		u64_t pinPolarity : 1;
		u64_t remoteIRR : 1;
		u64_t triggerMode : 1;
		u64_t mask : 1;
		u64_t reserved : 39;
		u64_t destination : 8;
	} regs;
	u64_t value;
} reg_redtbl_t;

enum redtlb_half
{
	REDTBL_LO,
	REDTBL_HI
};

enum DeliveryMode
{
	eEDGE = 0,
	eLEVEL = 1,
};

enum DestinationMode
{
	ePHYSICAL = 0,
	eLOGICAL = 1
};

typedef struct
{
	uint8_t id;
	uint32_t p_addr;
	uint32_t p_gsi;
	volatile virt_addr_t v_addr;
} ioapic_t;

/* Variável que mantém a estrutura do ioapic_t. Ela é modificada pelo "ioapic_ini(void)",
que atribui o endereço físico e virtual, a partir do ACPI. */
ioapic_t ioapic_ptr;

/* Variável a ser utilizada como mutex. */
CREATE_SPINLOCK(spinlock_ioapic);

/* Devolve o offset na tabela de redirecinamento, a partir do irq. O valor do irq já
deve estar redirecionado. */
static inline u8_t get_redtbl_offset(u8_t irq, enum redtlb_half half)
{
	return REDTBL_ENTRY(irq) + half;
}

/**
 * @brief  Faz leitura e retorna o registro apontado em reg_sel.
 * @note   Careful: will race
 * @param  ioapic_addr:
 * @param  reg_sel:
 * @retval
 */

static inline uint32_t ioapic_read(virt_addr_t ioapic_addr, uint8_t reg_sel)
{
	volatile mmio_ioapic_t *ioapic = (void *)ioapic_addr;

	/* tell IOREGSEL the register where we want to read from */
	ioapic->setReg = (reg_sel & 0xFF);

	/* return the data from IOWIN */
	return ioapic->getData;
}

/**
 * @brief  Escreve o valor data no registro indicado em reg_sel
 * @note   Careful: will race
 * @param  ioapic_addr:
 * @param  reg_sel:
 * @param  data:
 * @retval None
 */
static inline void ioapic_write(virt_addr_t ioapic_addr, uint8_t reg_sel, uint32_t data)
{
	volatile mmio_ioapic_t *ioapic = (void *)ioapic_addr;

	spinlock_lock(&spinlock_ioapic); /* Sincronização. */

	/* tell IOREGSEL where we want to read from */
	ioapic->setReg = (reg_sel & 0xFF);

	/* write the value to IOWIN */
	ioapic->getData = data;

	spinlock_unlock(&spinlock_ioapic);
}

static inline u32_t get_redtbl_entry_low(u8_t irq)
{
	u8_t offset = get_redtbl_offset(irq, REDTBL_LO);
	return ioapic_read(ioapic_ptr.v_addr, offset);
}
static inline u32_t get_redtbl_entry_high(u8_t irq)
{
	u8_t offset = get_redtbl_offset(irq, REDTBL_HI);
	return ioapic_read(ioapic_ptr.v_addr, offset);
}
u8_t ioapic_id(virt_addr_t ioapic_addr)
{
	return ((ioapic_read(ioapic_addr, IOAPIC_ID) >> 24) & 0xF);
}

/**
 * @brief  Devolve a versão do I/P APIC
 * @note   IOPCIVER
 * @param  ioapic_id:
 * @retval
 */
static inline uint32_t ioapic_ver(void)
{
	return (ioapic_read(ioapic_ptr.v_addr, IOAPIC_VER) & 0xFF);
}
/**
 * @brief  Extrai a quantidade de IRQs que podem ser manipuladas pelo I/O APIC
 * @note   IOPCIVER
 * @param  ioapic_id:
 * @retval
 */
static inline uint32_t ioapic_max_redirect_entry(void)
{
	return (ioapic_read(ioapic_ptr.v_addr, IOAPIC_VER) >> 16) & 0xFF;
}

static inline uint32_t ioapic_arbitration_id(void)
{
	return (ioapic_read(ioapic_ptr.v_addr, IOAPIC_ARB) >> 24) & 0xF;
}
static inline u32_t ioapic_gsi_base(void)
{
	u32_t ioapic_id = hw_info.ioapic[cpu_id()].id;
	return hw_info.ioapic[ioapic_id].gsi_base;
}
static void ioapic_redirect_irq(u8_t irq)
{
	// int8_t cpu = cpu_id();

	int8_t cpu = 0xFF;
	// uint32_t half0 = (uint32_t)(REMAP_IRQ(irq) | IOAPIC_IRQ_MASK | IO_APIC_DESTMOD | IO_APIC_DELMOD_LOW);
	uint32_t half0 = (uint32_t)(REMAP_IRQ(irq) | IOAPIC_IRQ_MASK | IO_APIC_DELMOD_LOW);

	uint32_t half1 = cpu << 24;

	ioapic_write(ioapic_ptr.v_addr, get_redtbl_offset(irq, REDTBL_LO), half0);
	ioapic_write(ioapic_ptr.v_addr, get_redtbl_offset(irq, REDTBL_HI), half1);
}

static void ioapic_redirect_table(void)
{
	// Pego o número máximo de IRQ's que podem ser redirecionadas
	uint32_t ioapic_maxirq = ioapic_max_redirect_entry() + 1;

	// Faço um loop para redirecionar todos os IRQ's, vinculando todos ao BSP
	for (uint8_t i = ioapic_gsi_base(); i < ioapic_maxirq; i++)
	{
		ioapic_redirect_irq(i);
	}
}
/**
 * @brief  Initialised the (assumed) wirings for the legacy PIC IRQs
 * @note   Send all IRQs to the BSP for simplicity
 * @retval None
 */

void ioapic_ini(void)
{
	ioapic_ptr.id = hw_info.ioapic[cpu_id()].id;
	ioapic_ptr.p_addr = hw_info.ioapic[ioapic_ptr.id].addr;

	// Faço o mapeamento da página física onde se encontra o registro do lapic
	u64_t phys_map = (u64_t)align_down(ioapic_ptr.p_addr, PAGE_SIZE);
	void *virt_map = kfixmap(FIX_IO_APIC_BASE);

	ioapic_ptr.v_addr = set_fixmap_nocache(FIX_IO_APIC_BASE, phys_map);

	ioapic_redirect_table();
	apic_eoi();
}

void irq_mask(int8_t irq_redirect, int8_t cpunum)
{
	u8_t irq = UNREMAP_IRQ(irq_redirect);
	u32_t half0 = get_redtbl_entry_low(irq) | IOAPIC_IRQ_MASK;

	ioapic_write(ioapic_ptr.v_addr, get_redtbl_offset(irq, REDTBL_LO), half0);
}

void irq_umask(int8_t irq_redirect, int8_t cpunum)
{
	u8_t irq = UNREMAP_IRQ(irq_redirect);
	uint32_t half0 = get_redtbl_entry_low(irq);
	uint32_t value = (uint32_t)(half0 | IOAPIC_IRQ_MASK) ^ IOAPIC_IRQ_MASK;

	ioapic_write(ioapic_ptr.v_addr, get_redtbl_offset(irq, REDTBL_LO), (uint32_t)value);
}

void dump_ioapic(void)
{
	uint32_t ioapic_maxirq = ioapic_max_redirect_entry();

	for (uint8_t i = 0; i < ioapic_maxirq; i++)
	{
		kprintf("\n%d = %x", i, ioapic_read(ioapic_ptr.v_addr, REDTBL_ENTRY(i)));
	}
}