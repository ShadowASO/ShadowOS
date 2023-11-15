/*--------------------------------------------------------------------------
*  File name:  smp.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 23-11-2022
*--------------------------------------------------------------------------
Rotinas para a inicialização de todos os cores do processador.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"
#include "sync/atomic.h"
#include "info.h"
#include "lapic.h"
#include "mm/page.h"
#include "hpet.h"
#include "mm/vmm.h"
#include "mm/pgtable_types.h"
#include "smp.h"
#include "../drivers/graphic/pixmap.h"
#include "scheduler.h"
// #include "syscall/syscall_gen.h"
#include "syscall/syscalls.h"

volatile bool smp_ap_started_flag = false;

volatile u8_t percpu_init_ap_ok = 0;
volatile u8_t idt_load_ok = 0;
volatile u8_t smp_ap_kmain_ok = 1;
volatile virt_addr_t smp_ap_stack = NULL;

static u64_t flags_mmap_smp = PG_FLAG_P | PG_FLAG_W | PG_FLAG_NC;

atomic32_t smp_nr_cpus_ready = {1};

/**
 * @brief O core que executa esta rotina verifica se o seu Local Apic está configurado e na
 * lista de APICS.
 *
 * @return cpuid_t
 */
cpuid_t smp_cpu_id_full(void)
{
    cpuid_t lid = cpu_id();
    for (size_t i = 0; i < apic_system.nr_lapic; i++)
        if (apic_system.lapic_list[i].id == lid)
            return i;
    kprintf("CPU not found in lapic_list");
    return 0;
}

static void smp_boot_ap(size_t index)
{
    lapic_core_t *lapic = &apic_system.lapic_list[index];

    if (smp_ap_stack == NULL)
    {
        phys_addr_t stack_top = alloc_frames(zone_normal_id(), 2); // aloca 16K

        phys_addr_t stack_button = stack_top + (PAGE_SIZE * 4);
        smp_ap_stack = phys_to_virt(stack_button - 16);
        memset(phys_to_virt(stack_top), 0, (PAGE_SIZE * 4));
        kprintf("\nAP[%d]: smp_ap_stack=%p.", index, smp_ap_stack);
    }
    // debug_pause("P1");

    // Set by the AP when initialisation is complete
    smp_ap_started_flag = 0;

    /*--------------------*/

    // Adapted from https://nemez.net/osdev/lapic.txt
    // Send the INIT IPI
    send_apic_ipi(lapic->id, IPI_INIT);
    hpet_sleep_milli(10);

    // Send the SIPI (first attempt)
    /**
     * Aqui eviamos uma mensagem para determinado core(dest_id) com o "Delivery Mode"
     * 110(Start-Up). Enviamos também um Vetor(índice) que aponta o page frame, onde
     * inicia o código a ser executado. Como se trata de uma interrupção utilizada na
     * inicialização dos cores, a execução é em real modo e deve atuar diretamente na
     * memória física, com limitações de endereçamento em 1M. O vetor 1 aponta para o
     * endereço 0x1000, o 2 para 0x2000 e assim por diante.
     *
     */

    send_apic_ipi(lapic->id, IPI_START_UP | ((uint32_t)TRAMPOLINE_START / PAGE_SIZE));
    hpet_sleep_milli(10);

    // debug_pause("P1");

    if (smp_ap_started_flag == 0)
    {
        // Send SIPI again (second attempt)
        send_apic_ipi(lapic->id, IPI_START_UP | ((uint32_t)TRAMPOLINE_START / PAGE_SIZE));
        hpet_sleep_milli(10);

        if (smp_ap_started_flag == 0)
        {
            kprintf("\nCPU %d failed to boot", index);
            lapic->present = 0;
            return;
        }
    }
    // É preciso analisar as rotinas de unmap e free neste caso, ou teremos um vazamento
    //  de memória.
    smp_ap_started_flag = 0;
    smp_ap_stack = NULL;
}

// Boots all the cores
void smp_init(void)
{
    kprintf("\nBSP [%d]: Ativando todos os CORES", cpu_id());

    /* Localização virtual do trampoline no código e o seu tamanho. */
    virt_addr_t ini_code = (virt_addr_t)&smp_trampoline_start;
    virt_addr_t pend_code = (virt_addr_t)&smp_trampoline_end;
    u32_t code_size = subptrs((pend_code), (ini_code));

    /* Local na memória física para onde vai ser copiado o trampoline. */
    phys_addr_t mm_base = TRAMPOLINE_START;
    phys_addr_t pend = round_up(mm_base + code_size, PAGE_SIZE);

    /* Mapeando a memória.*/
    for (size_t i = mm_base; i < pend; i += PAGE_SIZE)
    {
        set_page_allocated(i);
        kmap_frame((virt_addr_t)i, i, flags_mmap_smp);
    }
    // debug_pause("P1");
    memset((virt_addr_t)mm_base, 0, code_size);
    memcpy((virt_addr_t)mm_base, (virt_addr_t)ini_code, code_size);

    kprintf("\nBSP [%d]:Trampoline: code_size=%d\n", cpu_id(), code_size);

    /* Inicia a ativação de todos os Cores AP. */
    u8_t bsd_id = cpu_id();
    u8_t ap_id = 0;
    for (size_t i = 0; i < apic_system.nr_lapic; i++)
    {
        ap_id = apic_system.lapic_list[i].id;
        if (ap_id != bsd_id)
        {
            smp_ap_kmain_ok = 0;
            kprintf("\n\nAtivando AP[%d]:", ap_id);
            smp_boot_ap(i);

            /* Aguardo o tempo necessário para que o core seja ativado e inicializado. */
            hpet_sleep_milli(300);
        }
    }

    // Wait for other CPUs to fully finish initialisation
    /**
     * A variável global "smp_nr_cpus_ready" é incrementada por cada core
     * que conclui sua inicialização e salto para o modo protegido, e exe-
     * cutando a rotina smp_ap_kmain(void). Verificando o número de cores
     * iniciados, sabemos se a inicialização foi concluída.
     *
     */
    while (smp_nr_cpus() < apic_system.nr_lapic)
        ;

    kprintf("\n\nFinished AP boot sequence\n");
    kprintf("\nCORE Ativados: %d", apic_system.nr_lapic);
    kprintf("\nCORE BSP [%d]: ", bsd_id);
}

void smp_ap_kmain(void)
{
    uint32_t cpu_id = smp_cpu_id();

    // kprintf("\nAP %u: Iniciando lapic [%u] timer", cpu_id, id);

    apic_ini_ap();

    kprintf("\nAP[%d]: Habilitando interrupt", cpu_id);

    local_irq_enable();

    syscall_enable();

    atomic_inc_read32(&smp_nr_cpus_ready);
    kprintf("\nAP[%d]: Concluida.", cpu_id);

    // debug_pause("p1");
    scheduler_ap();
}
