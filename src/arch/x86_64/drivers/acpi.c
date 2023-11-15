/*--------------------------------------------------------------------------
*  File name:  acpi.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 29-04-2021
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a alocação de
páginas físicas.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "acpi.h"
#include "debug.h"
#include "stdio.h"
#include "string.h"
#include "screen.h"
#include "info.h"
#include "lapic.h"
#include "mm/fixmap.h"

// Ponteiro para a raiz de tabelas do ACPI.
rsdp_t *acpi_rsdp;

fadt_t acpi_fadt;

#define MAX_NMIS 40
madt_lapic_nmi_t *nmi_list[MAX_NMIS];
size_t nmi_list_size = 0;

/* check_hdr()
 *  - checksum
 *  - revision
 *  - optionally display OEMID, ...
 */
static int check_sum(void *hdr, size_t length)
{
    uint8_t sum = 0;
    uint8_t *p = (uint8_t *)hdr;
    unsigned i;

    for (i = 0; i < length; i++)
    {
        sum += p[i];
    }
    if (sum != 0)
    {
        kprintf("WARNING: checksum = %hhu (should be 0)\n", sum);
    }
    return sum;
}
static void *scan_rsdp(u64_t start, u64_t end)
{
    union
    {
        char str[9];
        uint32_t u32[2];
    } __attribute__((packed)) acpi_sig = {"RSD PTR "};

    uint32_t phys = start;
    uint32_t size = end - start;
    volatile const uint32_t *p = (uint32_t *)phys_to_virt(phys);
    unsigned i;

    for (i = 0; i < size / sizeof(uint32_t); i++)
    {
        if (p[i] == acpi_sig.u32[0] && p[i + 1] == acpi_sig.u32[1])
        {

            return (rsdp_t *)(void *)&p[i];
        }
    }

    return NULL;
}

/**
 * @brief Faz uma varredura nos endereços físicos, iniciando em 0xE0000 até 0xFFFFF
 * RSDP_SEARCH_START=0xE0000
 * RSDP_SEARCH_END=0xFFFFF
 * @return struct rsdp*
 */
static void *find_rsdp(void)
{

    // Scan the Extended BIOS Data Area
    uint16_t *ebda_ptr = (uint16_t *)phys_to_virt(0x40e);
    u64_t ebda = *ebda_ptr << 4;
    void *p = scan_rsdp(ebda, ebda + 1024);
    if (p)
    {
        kprintf("\nEBDA.");
        return p;
    }

    uint32_t phys = RSDP_SEARCH_START;
    uint32_t end = RSDP_SEARCH_END;

    // Scan 0xE0000 - 0xFFFFF
    p = scan_rsdp(phys, end);
    if (p)
    {
        kprintf("\nScan 0xE0000 - 0xFFFFF.");
        return p;
    }

    return 0;
}

// Every table we access needs to be called here first
static void map_table(u64_t phys_addr)
{
    u64_t *virt_align = phys_to_virt(phys_addr);
    char sig[5] = {0};

    // Encontro o endereço físico do frame e virtual, alinhados em 4096 bytes e
    // faço o mapeamento do frame com esse endereço físico
    // Map the first two pages for safety, then map the rest of the table
    // The header could cross a page boundary and generate a page fault.
    // Faço o mapeamento do frame onde se localiza a tabela rsdt - inclusive uma posterior,
    // por garantia.
    u64_t frame = (u64_t)align_down(phys_addr, PAGE_SIZE);
    rsdt_t *rsdt = phys_to_virt(frame);
    kmap_frame(rsdt, frame, acpi_flags);

    // Mapeio também o frame imediatamente superior ao endereço físico
    frame = (u64_t)align_up(phys_addr, PAGE_SIZE);
    rsdt = phys_to_virt(frame);
    kmap_frame(rsdt, frame, acpi_flags);

    acpi_header_t *header = (acpi_header_t *)virt_align;
}
static void add_nmi(madt_lapic_nmi_t *entry)
{
    if (nmi_list_size >= MAX_NMIS)
        return;
    nmi_list[nmi_list_size++] = entry;
    if (entry->processor_id == 0xFF)
        kprintf("\nNMI for all CPUs, LINT%d\n", entry->lapic_lint);
    else
        kprintf("\n[%d]:NMI for CPU %d, LINT%d\n", nmi_list_size, entry->processor_id, entry->lapic_lint);
}
/*
 * The MADT (Multiprocessor APIC Description Table), Signature APIC,
 * contains information about the number of enabled processors and
 * the I/O APICs.
 */
static int parse_madt(acpi_header_t *madt_header)
{
    kprintf("\n(*):%s(%d) - Fazendo o parse do MADT.", __FUNCTION__, __LINE__);

    madt_t *madt = (madt_t *)madt_header;

    if (check_sum(madt, madt->header.len) != 0)
    {
        kprintf("WARNING: checksum of MADT invalid.\n");
        return -1;
    }

    hw_info.lapic_adr = madt->lapic_adr;
    hw_info.cpu_cnt = 0;
    hw_info.ioapic_cnt = 0;

    /* O tamanho total da tabela madt, incluindo os registros, que possuem tamanho variável. */
    size_t size = (madt->header.len - sizeof(madt_t));

    /* Ponteiros temporários para navegar pelos registros do MADT.*/
    madt_entry_head_t *head;
    madt_entry_ioapic_t *eioapic;
    madt_entry_lapic_t *elapic;

    for (size_t offset = 0; offset < size; offset += head->len)
    {
        head = (void *)&madt->entry[offset];
        unsigned subtype = head->type; /* first byte is type of entry */

        switch (subtype)
        {
        case MADT_TYPE_LAPIC:

            elapic = (madt_entry_lapic_t *)(virt_addr_t)head;

            if (elapic->flags.enabled)
            {
                hw_info.cpu[elapic->apic_id].cpu_id = elapic->apic_id;
                hw_info.cpu[elapic->apic_id].lapic_adr = madt->lapic_adr;
                hw_info.cpu_cnt++;
                add_lapic(elapic);

                // kprintf("CPU id=%u  enabled: %u local APIC id: %u\n",
                //         (ptr_t)entry_lapic->acpi_processor_id, (ptr_t)entry_lapic->flags.enabled, (ptr_t)entry_lapic->apic_id);
            }
            break;
        case MADT_TYPE_IOAPIC:

            eioapic = (madt_entry_ioapic_t *)(virt_addr_t)head;

            hw_info.ioapic[eioapic->ioapic_id].id = eioapic->ioapic_id;
            hw_info.ioapic[eioapic->ioapic_id].addr = eioapic->ioapic_addr;
            hw_info.ioapic[eioapic->ioapic_id].gsi_base = eioapic->gsi_base;
            hw_info.ioapic_cnt++;

            // kprintf("I/O APIC id=%u  adr: 0x%x\n", (ptr_t)entry_ioapic->ioapic_id, (ptr_t)entry_ioapic->ioapic_addr);
            break;
        case MADT_NMI:
            add_nmi((madt_lapic_nmi_t *)(virt_addr_t)head);
            break;
        default:
            break;
        }
    }
    kprintf("\n(*):%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
    return 0;
}

static int parse_hpet(acpi_header_t *phpet)
{
    hpet_t *hpet = (hpet_t *)phpet;

    if (check_sum(hpet, hpet->header.len) != 0)
    {
        kprintf("WARNING: checksum of MADT invalid.\n");
        return -1;
    }

    int entries_size = (hpet->header.len - sizeof(acpi_header_t));
    int tmp_size = 0;

    for (int i = 0; i < MAX_HPET_BLOCK; i++)
    {
        hpet_entry_t *entry_hpet = (void *)&hpet->entry[i];

        hw_info.hpet[i].hpet_address = entry_hpet->address;
        hw_info.hpet[i].hpet_number = entry_hpet->hpet_number;
        hw_info.hpet[i].timers_count = entry_hpet->comparator_count;
        hw_info.hpet[i].minimum_tick = entry_hpet->minimum_tick;
        hw_info.hpet[i].suport_legacy = entry_hpet->legacy_replacement;
        hw_info.hpet[i].counter_size = entry_hpet->counter_size;

        kprintf("\nHPET - Bloco=%d", entry_hpet->hpet_number);
        tmp_size = tmp_size + entries_size;
        if (tmp_size >= entries_size)
        {
            break;
        }
    }

    return 0;
}

static int parse_pmtt(acpi_header_t *p_pmtt)
{
    pmtt_table_t *pmtt = (pmtt_table_t *)p_pmtt;

    if (check_sum(pmtt, pmtt->header.len) != 0)
    {
        kprintf("WARNING: checksum of PMTT invalid.\n");
        return -1;
    }

    int entries_size = (pmtt->header.len - sizeof(acpi_header_t));
    int entries_len = entries_size / sizeof(pmtt_aggregator_t);

    for (int i = 0; i < entries_len; i++)
    {
        pmtt_aggregator_t *entry_pmtt = (void *)&pmtt->entries[i];
        if (entry_pmtt->type == PMTT_DEVICE_SOCKET)
        {
            /* code */
            kprintf("\nSocket");
        }
        else if (entry_pmtt->type == PMTT_DEVICE_MEMORY)
        {
            kprintf("\nMemory");
            /* code */
        }
        else if (entry_pmtt->type == PMTT_DEVICE_DIMM)
        {
            kprintf("\nDIMM");
            /* code */
        }

        // kprintf("\nHPET - Bloco=%d", entry_hpet->hpet_number);
    }

    return 0;
}
/* Parser da FADT table. */
static int parse_fadt(acpi_header_t *htable)
{
    fadt_t *fadt = (fadt_t *)htable;
    fadt_t *p = &acpi_fadt;

    if (check_sum(fadt, fadt->header.len) != 0)
    {
        kprintf("WARNING: checksum of MADT invalid.\n");
        return -1;
    }
    memcpy(p, htable, sizeof(fadt_t));

    /* RTC century. */
    hw_info.rtc_timer.century = p->century;

    return 0;
}

static void parse_rsdt(void)
{
    kprintf("\n(*):%s(%d) - Fazendo o parse do RSDT.", __FUNCTION__, __LINE__);

    /* Encontro o endereço físico do frame e virtual, alinhados em 4096 bytes e
    faço o mapeamento do frame com esse endereço físico. */

    phys_addr_t base = (u64_t)align_down(acpi_rsdp->rsdt_adr, PAGE_SIZE);
    phys_addr_t pend = align_up(base + PAGE_SIZE, PAGE_SIZE);

    for (phys_addr_t start = base; start < pend; start += PAGE_SIZE)
    {
        set_page_allocated(start);
        kmap_frame(phys_to_virt(start), start, acpi_flags);
    }

    rsdt_t *rsdt = phys_to_virt(acpi_rsdp->rsdt_adr);
    if (check_sum(rsdt, rsdt->header.len) != 0)
    {
        kprintf("\nWARNING: checksum of ACPI RSDT is incorrect.");
        return;
    }

    char sign[5] = {0};
    int esize = (rsdt->header.rev >= 2) ? 8 : 4;
    virt_addr_t entry = rsdt->entry;
    u8_t len = (rsdt->header.len - sizeof(acpi_header_t)) / esize;

    phys_addr_t phys_addr = 0;
    acpi_header_t *htable = NULL;

    for (size_t i = 0; i < len; i++)
    {
        /* Verifica o tamanho das entry em bytes. */
        if ((rsdt->header.rev >= 2))
        {
            phys_addr = ((u64_t *)entry)[i];
        }
        else
        {
            phys_addr = ((u32_t *)entry)[i];
        }
        /*-------------------------------------------*/

        htable = phys_to_virt(phys_addr);

        memcpy(sign, htable->sign, 4);
        kprintf("\n(*) Table found = %s", sign);

        if (!memcmp(sign, MADT_SIGNATURE, 4))
        {
            parse_madt(htable);
        }
        else if (!memcmp(sign, HPET_SIGNATURE, 4))
        {
            parse_hpet((htable));
        }
        else if (!memcmp(sign, PMTT_SIGNATURE, 4))
        {
            kprintf("\nTable: %s", sign);
            parse_pmtt((htable));
        }
        else if (!memcmp(sign, FADT_SIGNATURE, 4))
        {
            kprintf("\nTable: %s", sign);
            parse_fadt(htable);
        }
    }
    // pause_enter();
    kprintf("\n(*):%s(%d) - Finalizando.", __FUNCTION__, __LINE__);
}

void setup_acpi(void)
{
    acpi_rsdp = find_rsdp();

    if (acpi_rsdp == NULL)
        kprintf("\nACPI: no RSDP found");

    if (check_sum(acpi_rsdp, 20) != 0)
    {
        kprintf("WARNING: checksum of ACPI RSDP is incorrect.");
    }
    kprintf("\nACPI: revision=%d", acpi_rsdp->rev);
    parse_rsdt();
    // pause_enter();
}
void dump_acpi_nmi(void)
{
    for (size_t i = 0; i < nmi_list_size; i++)
    {
        if (nmi_list[i]->processor_id == 0xFF)
            kprintf("\nNMI for all CPUs, LINT%d\n", nmi_list[i]->lapic_lint);
        else
            kprintf("\n[%d]:NMI for CPU %d, LINT%d\n", i, nmi_list[i]->processor_id, nmi_list[i]->lapic_lint);
    }
}
void dump_rsdp(rsdp_t *rsdp)
{

    kprintf("\nrsdp->signature=%s", rsdp->sign);
    kprintf("\nrsdp->checksum=%d", rsdp->checksum);
    kprintf("\nrsdp->oemid=%s", rsdp->oemid);
    kprintf("\nrsdp->revision=%x", rsdp->rev);
    kprintf("\nrsdp->rsdt_adr=%x", rsdp->rsdt_adr);
    kprintf("\nrsdp->length=%d", rsdp->length);
    kprintf("\nrsdp->xsdt_adr=%x", rsdp->xsdt_adr);
    kprintf("\nrsdp->extended_checksum=%x", rsdp->extended_checksum);
}

void dump_header(acpi_header_t *head)
{
    kprintf("\nhead->signature=%s", head->sign);
    kprintf("\nhead->checksum=%d", head->len);
    kprintf("\nhead->revision=%x", head->rev);
    kprintf("\nhead->checksum=%x", head->checksum);
    kprintf("\nhead->oemid=%s", head->oemid);
    kprintf("\nhead->oem_table_id=%x", head->oem_table_id);
    kprintf("\nhead->oem_revision=%x", head->oem_rev);
}