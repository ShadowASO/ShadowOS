/*--------------------------------------------------------------------------
*  File name:  pci.h
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 18-10-2022
*
*--------------------------------------------------------------------------
Este header reune estruturas para o Gerenciamento de Memória
--------------------------------------------------------------------------*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifndef _MM_PCI_H
#define _MM_PCI_H

#define PCI_HEADER_MAX_REGS 64

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

// Header type register
#define PCI_HEADER_TYPE_STANDARD 0x00
#define PCI_HEADER_TYPE_BRIDGE 0x01
#define PCI_HEADER_TYPE_CARDBUS 0x02

#define PCI_HEADER_TYPE_MF 0x80

typedef enum
{
    ePCI_HEADER_REG0 = 0x0,
    ePCI_HEADER_REG1 = 0x4,
    ePCI_HEADER_REG2 = 0x8,
    ePCI_HEADER_REG3 = 0xC,
    ePCI_HEADER_BAR0 = 0x10,
    ePCI_HEADER_BAR1 = 0x14,
    ePCI_HEADER_BAR2 = 0x18,
    ePCI_HEADER_BAR3 = 0x1C,
    ePCI_HEADER_BAR4 = 0x20,
    ePCI_HEADER_BAR5 = 0x24
} e_pci_header_offset_t;

typedef union
{
    struct
    {
        uint32_t vendor_id : 16;
        uint32_t device_id : 16;
    } reg0;

    struct
    {
        uint32_t command : 16;
        uint32_t status : 16;
    } reg1;

    struct
    {
        uint32_t rev_id : 8;
        uint32_t prog_if : 8;
        uint32_t subclass : 8;
        uint32_t class_code : 8;
    } reg2;

    struct
    {
        uint32_t cache_size : 8;
        uint32_t latency : 8;
        uint32_t header_type : 8;
        uint32_t bist : 8;
    } reg3;
    uint32_t value;
} __attribute__((aligned)) pci_header_t;

typedef struct PCIAddress
{
    u8_t bus;
    u8_t device;
    u8_t function;
} PCIAddress;

void checkDevice(uint8_t bus, uint8_t device);
void pci_probe(void);
const char *pci_device_get_class_name(const u8_t class_code, const u8_t sub_class);
PCIAddress pci_find_device(uint8_t base_class, uint8_t sub_class);
void set_mem_enable(PCIAddress pci_addr, bool enable);

// u8_t pci_io_prog_if(PCIAddress pci_addr);
void show_device(PCIAddress pci_addr);

//---- PCI BASE ADDRESSES
u32_t pci_io_bar0(PCIAddress pci_addr);
u32_t set_pci_io_bar0(PCIAddress pci_addr, u32_t value);

u32_t pci_io_bar_size(PCIAddress pci_addr, e_pci_header_offset_t offset);

#endif