/*--------------------------------------------------------------------------
 *  File name:  pci.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 18-10-2022
 *--------------------------------------------------------------------------
 *  Rotinas para inserir um caractere em uma strings, com ajuste no tamanho.
 *--------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "string.h"
#include "io.h"
#include "pci.h"
#include "stdio.h"
#include "debug.h"
#include "mm/pgtable_types.h"
#include "mm/mm.h"
#include "mm/vmm.h"
#include "../graphic/vga.h"

u64_t *virt_vga_address = NULL;
u32_t pci_header_buf[PCI_HEADER_MAX_REGS] = {0};

static u32_t parse_address_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    address = (uint32_t)((lbus << 16) | (lslot << 11) |
                         (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));

    return (address);
}
/**
 * @brief Retorna um registro de 32 bits do espaço de configuração do PCI, de
 * acordo com o offset informado: 0x0, 0x4, 0x8 etc.
 *
 * @param bus
 * @param slot
 * @param func
 * @param offset
 * @return uint32_t
 */
static uint32_t pci_read_config_header(PCIAddress pci_addr, uint8_t offset)
{
    // Preparo a configuração de leitura a ser utulizada.
    u32_t address = parse_address_config(pci_addr.bus, pci_addr.device, pci_addr.function, offset);
    __write_portd(PCI_CONFIG_ADDR, address);

    // Devolve um registro com 32 bits
    return (__read_portd(PCI_CONFIG_DATA));
}

static void pci_write_config_header(PCIAddress pci_addr, u8_t offset, u32_t reg)
{
    u32_t address = parse_address_config(pci_addr.bus, pci_addr.device, pci_addr.function, offset);
    __write_portd(PCI_CONFIG_ADDR, address);

    // Devolve um registro com 32 bits
    __write_portd(PCI_CONFIG_DATA, reg);
}
static void pci_read_config(PCIAddress pci_addr)
{
    for (int i = 0; i < PCI_HEADER_MAX_REGS; i++)
    {
        pci_header_buf[i] = pci_read_config_header(pci_addr, i);
    }
}

static u16_t pci_io_vendor_id(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG0);
    return header.reg0.vendor_id;
}
static u16_t pci_io_device_id(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG0);
    return header.reg0.device_id;
}
static u16_t pci_io_command(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG1);
    return header.reg1.command;
}
static void set_pci_io_command(PCIAddress pci_addr, u16_t command)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG1);
    header.reg1.command = command;

    pci_write_config_header(pci_addr, ePCI_HEADER_REG1, header.value);
}
static u16_t pci_io_status(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG1);
    return header.reg1.status;
}
static u8_t pci_io_rev_id(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG2);
    return header.reg2.rev_id;
}
static u8_t pci_io_prog_if(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG2);
    return header.reg2.prog_if;
}
static u8_t pci_io_subclass(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG2);
    return header.reg2.subclass;
}
static u8_t pci_io_class_code(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG2);
    return header.reg2.class_code;
}
static u8_t pci_io_cache_size(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG3);
    return header.reg3.cache_size;
}
static u8_t pci_io_latency(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG3);
    return header.reg3.latency;
}

static u8_t pci_io_header_type(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG3);
    return header.reg3.header_type;
}
static u8_t pci_io_bist(PCIAddress pci_addr)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG3);
    return header.reg3.bist;
}
u32_t pci_io_bar0(PCIAddress pci_addr)
{
    return pci_read_config_header(pci_addr, ePCI_HEADER_BAR0);
}
u32_t set_pci_io_bar0(PCIAddress pci_addr, u32_t value)
{
    pci_write_config_header(pci_addr, ePCI_HEADER_BAR0, value);
    return pci_read_config_header(pci_addr, ePCI_HEADER_BAR0);
}

u32_t pci_io_bar_size(PCIAddress pci_addr, e_pci_header_offset_t offset)
{
    u32_t bar_ant = pci_read_config_header(pci_addr, offset);

    u32_t bar_new = set_pci_io_bar0(pci_addr, 0xfffffff0);
    u32_t size = (~(bar_new & 0xfffffff0) + 1);

    set_pci_io_bar0(pci_addr, bar_ant);
    return size;
}
static u32_t pci_io_header_bar1(PCIAddress pci_addr)
{
    return pci_read_config_header(pci_addr, ePCI_HEADER_BAR1);
}
static u32_t pci_io_header_bar2(PCIAddress pci_addr)
{
    return pci_read_config_header(pci_addr, ePCI_HEADER_BAR2);
}
static u32_t pci_io_header_bar3(PCIAddress pci_addr)
{
    return pci_read_config_header(pci_addr, ePCI_HEADER_BAR3);
}
static u32_t pci_io_header_bar4(PCIAddress pci_addr)
{
    return pci_read_config_header(pci_addr, ePCI_HEADER_BAR4);
}
static u32_t pci_io_header_bar5(PCIAddress pci_addr)
{
    return pci_read_config_header(pci_addr, ePCI_HEADER_BAR5);
}

static uint16_t pciCheckVendor(uint8_t bus, uint8_t device)
{
    PCIAddress pci_addr = {bus, device, 0};
    uint16_t vendor, device_id;
    /* Try and read the first configuration register. Since there are no
     * vendors that == 0xFFFF, it must be a non-existent device. */
    if ((vendor = pci_read_config_header(pci_addr, 0)) != 0xFFFF)
    {
        device_id = pci_read_config_header(pci_addr, 2);
    }
    return (vendor);
}
static void checkBus(uint8_t bus)
{
    uint8_t device;

    for (device = 0; device < 32; device++)
    {
        checkDevice(bus, device);
    }
}
static void showVGA(void *vga_virt)
{
}

void set_mem_enable(PCIAddress pci_addr, bool enable)
{
    pci_header_t header = {0};
    header.value = pci_read_config_header(pci_addr, ePCI_HEADER_REG1);
    u16_t command = header.reg1.command;

    /* Mem space enable, IO space enable, bus mastering. */
    const u16_t flags = 0x0007;

    if (enable)
    {
        command |= flags;
    }
    else
    {
        command &= ~flags;
    }
    set_pci_io_command(pci_addr, command);
}

void show_device(PCIAddress pci_addr)
{

    u8_t baseClass = pci_io_class_code(pci_addr);
    u8_t subClass = pci_io_subclass(pci_addr);
    uint16_t vendor = pci_io_vendor_id(pci_addr);
    uint16_t header = pci_io_header_type(pci_addr);
    uint32_t bar0 = pci_io_bar0(pci_addr);

    uint16_t header_type = pci_io_header_type(pci_addr);
    u8_t isMF = (header_type & PCI_HEADER_TYPE_MF);
    u8_t isBridge = (header_type & PCI_HEADER_TYPE_BRIDGE);

    kprintf("\n\nBUS[%x] - DEVICE[%x] - FUNC[%x]-->VENDOR[%x]", pci_addr.bus, pci_addr.device, pci_addr.function, vendor);

    kprintf("\nCLASS[%x] - SUBCLASS[%x] - %s", baseClass, subClass, pci_device_get_class_name(baseClass, subClass));
    if ((baseClass == 0x6) && (subClass == 0x4))
    {
        // bridge
    }
    else
    {
        // set_mem_enable(pci_addr, false);

        kprintf("\nBAR0=%x", bar0);
        kprintf("\nBAR1=%x", pci_io_header_bar1(pci_addr));
        kprintf("\nBAR2=%x", pci_io_header_bar2(pci_addr));
        kprintf("\nBAR3=%x", pci_io_header_bar3(pci_addr));
        kprintf("\nBAR4=%x", pci_io_header_bar4(pci_addr));
        kprintf("\nBAR5=%x", pci_io_header_bar5(pci_addr));
        kprintf("\nProg IF=%x", pci_io_prog_if(pci_addr));
        kprintf("\nReg: Command=%x", pci_io_command(pci_addr));
        kprintf("\nBar0 Size=%x", pci_io_bar_size(pci_addr, ePCI_HEADER_BAR0));

        // virt_vga_address = phys_to_virt(bar0 & 0xFFFFFFF0);
        // kmap_frame(virt_vga_address, (bar0 & 0xFFFFFFF0), (PG_FLAG_P | PG_FLAG_W | PG_FLAG_G));
        //  showVGA(virt_vga_address);

        // set_mem_enable(pci_addr, false);
    }
    pause_enter();
}

static void checkFunction(uint8_t bus, uint8_t device)
{
    uint8_t baseClass;
    uint8_t subClass;
    uint8_t secondaryBus;
    PCIAddress pci_addr = {bus, device, 0};
    uint16_t vendor_id = pci_io_vendor_id(pci_addr);
    uint16_t header_type = pci_io_header_type(pci_addr);

    u8_t isMF = (header_type & PCI_HEADER_TYPE_MF);

    if (true)
    {

        for (uint32_t function = 0; function < 8; function++)
        {
            pci_addr.function = function;

            vendor_id = pci_io_vendor_id(pci_addr);
            baseClass = pci_io_class_code(pci_addr);
            subClass = pci_io_subclass(pci_addr);

            if (vendor_id == 0xFFFF)
            {
                continue;
            }
            else if ((baseClass == 0x6) && (subClass == 0x4))
            {
                // secondaryBus = getSecondaryBus(bus, device, function);
                // checkBus(secondaryBus);
                show_device(pci_addr);
            }
            else
            {
                show_device(pci_addr);
            }
        }
    }
}

void checkDevice(uint8_t bus, uint8_t device)
{
    PCIAddress pci_addr = {bus, device, 0};
    uint16_t vendor_id = pci_io_vendor_id(pci_addr);
    if (vendor_id == 0xFFFF)
    {
        return;
    }

    checkFunction(bus, device);
}

void pci_probe(void)
{
    for (int bus = 0; bus < 256; bus++)
    {
        for (int slot = 0; slot < 32; slot++)
        {
            checkDevice(bus, slot);
        }
    }
}

PCIAddress pci_find_device(uint8_t base_class, uint8_t sub_class)
{
    uint8_t baseClass;
    uint8_t subClass;
    // uint8_t secondaryBus;

    uint16_t vendor_id;
    uint16_t header_type;
    PCIAddress pci_addr = {0};

    // u8_t isMF = (header_type & PCI_HEADER_TYPE_MF);

    for (int bus = 0; bus < 256; bus++)
    {
        pci_addr.bus = bus;
        for (int slot = 0; slot < 32; slot++)
        {
            pci_addr.device = slot;
            for (uint32_t function = 0; function < 8; function++)
            {
                pci_addr.function = function;

                vendor_id = pci_io_vendor_id(pci_addr);
                baseClass = pci_io_class_code(pci_addr);
                subClass = pci_io_subclass(pci_addr);

                if (vendor_id == 0xFFFF)
                {
                    continue;
                }
                else if ((baseClass == base_class) && (subClass == sub_class))
                {
                    return pci_addr;
                }
            }
        }
    }
    return (PCIAddress){0, 0, 0};
}
