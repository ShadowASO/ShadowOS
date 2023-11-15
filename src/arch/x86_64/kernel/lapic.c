/*--------------------------------------------------------------------------
 *  File name:  lapic.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 05-06-2021
 *  Data revisão: 19-05-2022
 *--------------------------------------------------------------------------
 *  Rotina para configuração e uso do APIC
 *--------------------------------------------------------------------------*/
#include "ctype.h"
#include "bits.h"
#include "lapic.h"
#include "stdio.h"
#include "io.h"
#include "mm/vmm.h"
#include "mm/page.h"
#include "debug.h"
#include "pit.h"
#include "pic.h"
#include "hpet.h"
#include "config.h"
#include "kcpuid.h"
#include "mm/fixmap.h"
#include "interrupt.h"
#include "acpi.h"
#include "percpu.h"
#include "msr.h"
#include "scheduler.h"
#include "sync/spin.h"

// Contém os endereços físico e virtual do lapic
// lapic_base_t lapic_base;

// Contém uma descrição de todos os Apics presentes e ativados
// lapic_core_t lapic_list[MAX_LAPIC];
// size_t lapic_list_size = 0;

CREATE_SPINLOCK(spinlock_timer)

apic_sys_t apic_system;

//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
//              ROTINAS BASEADAS NO CPUID
//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
/** returns a 'true' value if the CPU supports APIC
 *  and if the local APIC hasn't been disabled in MSRs
 *  note that this requires CPUID to be supported.
 */
bool cpu_support_apic(void)
{
    cpuid_regs_t cpuid_var = cpuid_get(CPUID_GETFEATURES);
    return (cpuid_var.edx & CPUID_FEAT_EDX_APIC);
}

/**
 * Verifica se há suporte para o x2APIC mode.
 */
bool cpu_support_x2apic(void)
{
    cpuid_regs_t cpuid_var = cpuid_get(CPUID_GETFEATURES);
    return (cpuid_var.ecx & CPUID_FEAT_ECX_x2APIC);
}

//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
//              ROTINAS BASEADAS NO IA32_APIC_BASE MSR
//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
/**
 * @brief Devolve integralmente o registro IA32_APIC_BASE_MSR
 */
u64_t apic_base_msr(void)
{
    return (msr_read(IA32_APIC_BASE_MSR) | 0xFFF) ^ 0xFFF;
}

void set_apic_base_msr(u64_t value)
{
    msr_write(IA32_APIC_BASE_MSR, MSR_APIC_BASE_RESET);
    msr_write(IA32_APIC_BASE_MSR, value);
}

bool is_global_apic_enabled(void)
{
    apic_base_msr_t base = {.value = msr_read(IA32_APIC_BASE_MSR)};
    return base.fields.enabled;
}
static void global_apic_enable(void)
{
    u64_t eax, edx;
    set_apic_base_msr(apic_base_msr() | BIT(11));
}
static void global_apic_disable(void)
{
    u64_t eax, edx;
    set_apic_base_msr(apic_base_msr() ^ BIT(11));
}
bool is_bsp(void)
{
    apic_base_msr_t base = {.value = msr_read(IA32_APIC_BASE_MSR)};
    return base.fields.bsp;
}
//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
//        ROTINAS BASEADAS NOS REGISTROS DO APIC
//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

/**
 * @brief Ler o registro do local APIC e devolve o valor indicado no offset
 */
uint32_t apic_read(uint32_t reg_offset)
{
    size_t offset = reg_offset / 4;
    return apic_system.virt_base[offset];
}
/**
 * @brief Grava o valor no registro indicado pelo reg_offset
 *
 * @param reg_offset
 * @param value
 */
void apic_write(uint32_t reg_offset, uint32_t value)
{
    size_t offset = reg_offset / 4;
    apic_system.virt_base[offset] = value;
}

u8_t apic_id(void)
{
    uint32_t value = apic_read(APIC_ID);
    return (value >> 24) & 0xFF;
}

u8_t apic_version(void)
{
    u8_t value = apic_read(APIC_VER);
    return (value & 0xFF);
}
/**
 * @brief  Sinaliza a finalização de uma interrupção.
 * @note   Se essa sinalização não for realizada, o sistema para de
 * emitir interrupções.
 * @retval None
 */
void apic_eoi(void)
{
    apic_write(APIC_EOI, 0);
}

bool is_apic_enabled()
{
    uint32_t value = apic_read(APIC_SPU);
    return (value & MASK_SPU_ENABLE);
}
void apic_enable(void)
{
    uint32_t value = apic_read(APIC_SPU);
    apic_write(APIC_SPU, value | MASK_SPU_ENABLE);
}
void apic_disable(void)
{
    uint32_t value = apic_read(APIC_SPU);
    apic_write(APIC_SPU, (value | MASK_SPU_ENABLE) ^ MASK_SPU_ENABLE);
}
u8_t apic_max_lvt_entry(void)
{
    u32_t value = apic_read(APIC_VER);
    return (value >> 16) & 0xFF;
}

static void apic_synchronizing_ids(void)
{
    icr_entry_t icr = {0};
    u32_t value = 0;
    /*
        icr.fields.vector = 0;
        icr.fields.delivery_mode = ICR_DELIVERY_MODE_INIT;
        icr.fields.dest_mode = ICR_DESTINATION_PHYS;
        icr.fields.level = ICR_LEVEL_DEASSERT;
        icr.fields.trigger_mode = ICR_TRIGGER_MODE_LEVEL;
        icr.fields.dest_id = 0;
        icr.fields.shorthand = ICR_SHORTHAND_ALL;
        */
    icr.value = ICR_INIT | ICR_PHYSICAL | ICR_DEASSERT | ICR_LEVEL | ICR_ALL_EXC_SELF;

    apic_write(APIC_ICR_HIGH, icr.half.dw_high);
    apic_write(APIC_ICR_LOW, icr.half.dw_low);
}

static void lvt_error_enable(void)
{
    apic_write(APIC_LVT_ERROR, ISR_VECTOR_ERROR);
    apic_write(APIC_ESR, 0);
}

/**
 * @brief Envia a mensagem codificada em "vector_flags" para o
 * destinatário "id_dest"
 *
 * @param id_dest - high
 * @param vector_flags - low
 */
void send_apic_ipi(uint8_t dest_id, uint32_t vector_flags)
{
    icr_entry_t icr = {0};
    icr.fields.dest_id = dest_id;
    icr.half.dw_low = vector_flags;

    apic_write(APIC_ICR_HIGH, icr.half.dw_high);
    apic_write(APIC_ICR_LOW, icr.half.dw_low);
}
void apic_send_icr_msg(icr_entry_t icr)
{

    apic_write(APIC_ICR_HIGH, icr.half.dw_high);
    apic_write(APIC_ICR_LOW, icr.half.dw_low);
}
bool is_ipi_completed(void)
{
    icr_entry_t icr = {0};
    icr.half.dw_low = apic_read(APIC_ICR_LOW);
    return (icr.fields.delivery_status == 0);
}

static int get_maxlvt(void)
{
    unsigned int v, ver, maxlvt;

    v = apic_read(APIC_VER);
    maxlvt = GET_APIC_MAXLVT(v);
    return maxlvt;
}
static void clear_local_APIC(void)
{
    int maxlvt;
    unsigned int v;

    maxlvt = get_maxlvt();

    /*
     * Masking an LVT entry on a P6 can trigger a local APIC error
     * if the vector is zero. Mask LVTERR first to prevent this.
     */
    if (maxlvt >= 3)
    {
        v = ISR_VECTOR_ERROR; /* any non-zero vector will do */
        apic_write(APIC_LVT_ERROR, v | MASK_LVT_DISABLE);
    }
    /*
     * Careful: we have to set masks only first to deassert
     * any level-triggered sources.
     */
    v = apic_read(APIC_LVT_TIMER);
    apic_write(APIC_LVT_TIMER, v | MASK_LVT_DISABLE);
    v = apic_read(APIC_LVT_LINT0);
    apic_write(APIC_LVT_LINT0, v | MASK_LVT_DISABLE);
    v = apic_read(APIC_LVT_LINT0);
    apic_write(APIC_LVT_LINT1, v | MASK_LVT_DISABLE);
    if (maxlvt >= 4)
    {
        v = apic_read(APIC_LVT_PERFOR);
        apic_write(APIC_LVT_PERFOR, v | MASK_LVT_DISABLE);
    }

    /*
     * Clean APIC state for other OSs:
     */
    apic_write(APIC_LVT_TIMER, MASK_LVT_DISABLE);
    apic_write(APIC_LVT_LINT0, MASK_LVT_DISABLE);
    apic_write(APIC_LVT_LINT1, MASK_LVT_DISABLE);
    if (maxlvt >= 3)
        apic_write(APIC_LVT_ERROR, MASK_LVT_DISABLE);
    if (maxlvt >= 4)
        apic_write(APIC_LVT_PERFOR, MASK_LVT_DISABLE);

    apic_write(APIC_ESR, 0);
    apic_read(APIC_ESR);
}
/**
 * Faz a inicialização do APIC, com a paginação do endereço físico
 */
void setup_apic(void)
{
    kprintf("\n(*):%s(%d) - Fazendo o setup do APIC.", __FUNCTION__, __LINE__);

    /* Desabilita o PIC */
    kprintf("\nDesabilitando o i8259 IRQ chip.");
    pic_disable();

    if (!cpu_support_apic())
    {
        kprintf("\nNenhum APIC encontrado.");
    }

    uint8_t id = cpu_id(); /* O local APIC(BSP) que está realizando a configuração inicial. */

    apic_system.bsp_id = id;
    apic_system.phys_base = apic_base_msr();
    apic_system.virt_base = set_fixmap_nocache(FIX_APIC_BASE, apic_system.phys_base);

    add_handler_irq(ISR_VECTOR_ERROR, error_interrupt_handler);
    add_handler_irq(ISR_VECTOR_SPURIOUS, spurious_interrupt_handler);

    kprintf("\n(*):%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}
/* Inicializa o APIC no BSP. */
void apic_ini(void)
{
    uint8_t id = apic_id();
    u32_t value = 0;

    clear_local_APIC();

    /* Enable the LAPIC via the spurious interrupt register */
    value = apic_read(APIC_SPU);
    value &= ~APIC_VECTOR_MASK;
    value |= MASK_SPU_ENABLE;
    value |= MASK_SPU_FOCUS_DISABLE;
    value |= ISR_VECTOR_SPURIOUS;
    apic_write(APIC_SPU, value);

    value = apic_read(APIC_TPR);
    value &= ~MASK_TPR;
    apic_write(APIC_TPR, value);

    /*Set up the virtual wire mode.*/
    apic_write(APIC_LVT_LINT0, ICR_EXTINT);
    value = ICR_NMI;
    apic_write(APIC_LVT_LINT1, value);

    lvt_error_enable();

    /* Obtenho a precisão apenas do timer do local apic do BSP.
    Servirá para todos os apics locais.  */
    uint32_t ticks_ms = lapic_calibrate_timer();

    // Guardo as informações do local apic do core atual

    apic_system.lapic_list[id].id = id;
    apic_system.lapic_list[id].present = true;
    // apic_system.ticks_per_ms = ticks_ms;
    // apic_system.lapic_list[id].ticks_per_ms = ticks_ms;

    // Limpo as interrupções pendentes.
    apic_eoi();
    apic_synchronizing_ids();
}

/**
 * @brief  Habilita o funcionamento do Local APIC
 * @note  Cada core precisa iniciar o seu apic
 * @retval None
 */
void apic_ini_ap(void)
{
    // Guarda o ID do local APIC do core atual
    uint8_t id = apic_id();
    u32_t value = 0;

    value = apic_read(APIC_TPR);
    value &= ~MASK_TPR;
    apic_write(APIC_TPR, value);

    // Enable the LAPIC via the spurious interrupt register

    value = apic_read(APIC_SPU);
    value &= ~APIC_VECTOR_MASK;
    value |= MASK_SPU_ENABLE;
    value |= MASK_SPU_FOCUS_DISABLE;
    value |= ISR_VECTOR_SPURIOUS;
    apic_write(APIC_SPU, value);

    lvt_error_enable();

    // Guardo as informações do local apic do core atual
    apic_system.lapic_list[id].id = id;
    apic_system.lapic_list[id].present = true;

    // Limpo as interrupções pendentes.
    apic_eoi();
    apic_synchronizing_ids();
}

/*------------------------------------------------------------------------------------
                      APIC TIMER
--------------------------------------------------------------------------------------*/
/*
    Verifica se o Timer do Apic executa numa taxa constante e regular.
*/
bool is_apic_timer_constant(void)
{
    cpuid_regs_t cpuid_var = cpuid_get(0x6);
    return (cpuid_var.eax & 0x2);
}
/**
 * @brief  Iniciar o timer do atual core, numa frequência predefinida
 * @note   O Timer do APIC não utiliza o I/O Apic como intermediário.
 * Ele envia a interrupção diretametne ao seu processador.
 * Cada core precisa iniciar o seu timer
 * @retval None
 */
void init_lapic_timer(uint32_t ms)
{
    spinlock_lock(&spinlock_timer);

    uint8_t id = apic_id();
    uint32_t ticks_ms = lapic_calibrate_timer();
    uint32_t inicial_count = ticks_ms * ms;
    kprintf("\nLAPIC-TIMER[ %u ] - Freq:[ %d ] ticks/mill - inicial_count=%u Ticks", id, ticks_ms, inicial_count);

    spinlock_unlock(&spinlock_timer);

    /* Indicamos o número do vector que será utilizando
    como IRQ quando o Timer provocar uma interrupção.*/

    apic_write(APIC_LVT_TIMER, ISR_VECTOR_TIMER | TIMER_PERIODIC);

    /* Tell APIC timer to use divider 16 */
    apic_write(APIC_TIMER_DIVIDE_CONFIG, TIMER_DIVISOR);

    /* Ao gravarmos o valor inicial, a contagem inicial. Um valor 0 para o Timer.*/
    apic_write(APIC_TIMER_INIT_COUNT, inicial_count);

    /* acknoledge any pending interrupts */
    apic_eoi();
}

/*
    Calcula e retorna o número de ticks do timer do APIC em um milissegundo,
    utilizando o relógio de alta precisão HPET.
*/
uint32_t lapic_calibrate_timer(void)
{
    const uint32_t time_test_ms = 100;
    const uint32_t ticks_initial = 0xFFFFFFFF;
    uint32_t ticks_final = 0;
    uint32_t ticks_ms = 0;
    uint8_t id = apic_id();

    //********
    // Tell APIC timer to use divider
    apic_write(APIC_TIMER_DIVIDE_CONFIG, TIMER_DIVISOR);
    apic_write(APIC_LVT_TIMER, ISR_VECTOR_TIMER | TIMER_PERIODIC);
    apic_write(APIC_TIMER_INIT_COUNT, ticks_initial);

    // Utilizo o relógio do HPET para ajustar o time do APIC
    hpet_sleep_milli(time_test_ms);

    // Desativo o timer do apic
    apic_write(APIC_LVT_TIMER, MASK_LVT_DISABLE);

    // Faço a leitura do número de ticks no período
    ticks_final = apic_read(APIC_TIMER_CURR_COUNT);
    uint32_t ticks_total = ticks_initial - ticks_final;
    ticks_ms = (ticks_initial - ticks_final) / time_test_ms;

    // kprintf("\nlapic-ID=%u timer: ticks_em %u=%u - ticks_1ms=%u", id, test_ms, ticks_total, ticks_ms);
    //  kprintf("\nlapic-ID=%u timer: ticks_final=%u - ticks_inicial=%u", id, ticks_final, ticks_initial);
    //  debug_pause("p1");

    return ticks_ms;
}
void add_lapic(madt_entry_lapic_t *entry)
{
    if (apic_system.nr_lapic >= MAX_LAPIC)
        return;

    u8_t apic_id = entry->apic_id;

    apic_system.lapic_list[apic_id].present = (entry)->flags.enabled;
    apic_system.lapic_list[apic_id].id = entry->apic_id;
    apic_system.lapic_list[apic_id].acpi_id = entry->acpi_id;

    apic_system.nr_lapic++;
    kprintf("APIC: Detected local APIC, id %d\n", entry->apic_id);
}
/*
 * This interrupt should _never_ happen with our APIC/SMP architecture
 */
void spurious_interrupt_handler(cpu_regs_t *tsk_contxt)
{
    unsigned int v;
    // irq_enter();
    /*
     * Check if this really is a spurious interrupt and ACK it
     * if it is a vectored one.  Just in case...
     * Spurious interrupts should not be ACKed.
     */
    v = apic_read(APIC_ISR + ((ISR_VECTOR_SPURIOUS & ~0x1f) >> 1));
    if (v & (1 << (ISR_VECTOR_SPURIOUS & 0x1f)))
        apic_eoi();

#if 0
	static unsigned long last_warning; 
	static unsigned long skipped; 

	/* see sw-dev-man vol 3, chapter 7.4.13.5 */
	if (time_before(last_warning+30*HZ,jiffies)) { 
		printk(KERN_INFO "spurious APIC interrupt on CPU#%d, %ld skipped.\n",
		       smp_processor_id(), skipped);
		last_warning = jiffies; 
		skipped = 0;
	} else { 
		skipped++; 
	}
#endif
    // irq_exit();
}
/*
 * This interrupt should never happen with our APIC/SMP architecture
 */

void error_interrupt_handler(cpu_regs_t *tsk_contxt)
{
    unsigned int v, v1;

    // irq_enter();

    /* First tickle the hardware, only then report what went on. -- REW */
    v = apic_read(APIC_ESR);
    apic_write(APIC_ESR, 0);
    v1 = apic_read(APIC_ESR);

    apic_eoi();

    // atomic_inc(&irq_err_count);

    /* Here is what the APIC error bits mean:
       0: Send CS error
       1: Receive CS error
       2: Send accept error
       3: Receive accept error
       4: Reserved
       5: Send illegal vector
       6: Received illegal vector
       7: Illegal register address
    */
    kprintf("APIC error on CPU%d: %02x(%02x)\n",
            cpu_id(), v, v1);

    // irq_exit();
}

/**
 * @brief  Lista todos os registros diferentes de zero
 * @note
 * @retval None
 */

void dump_apic_registers()
{
    uint32_t reg = 0;
    int max_reg = 0x03f0;
    kprintf("\nIniciando dump do APIC Register:");
    for (int offset = 0; offset <= max_reg; (offset = offset + 0x10))
    {
        reg = apic_read(offset);
        if (reg != 0)
        {
            kprintf("\noffset=%x -- REG_VALUE=%x", offset, reg);
        }
    }
}
