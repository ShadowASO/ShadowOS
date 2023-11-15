/*-------------------------------------------------------------------------
 *  File name:  debug.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 13-01-2021
 *--------------------------------------------------------------------------
 *  Rotinas diversas.
 *--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "stdlib.h"
#include "stdio.h"
#include "debug.h"
#include "string.h"
#include "list.h"
#include "ktypes.h"

bool __debuging = false;

PRIVATE void print_int(int64_t intNum, int base);

/**
 * @brief Imprime um inteiro u64_t em seu formato binário
 *
 * @param x
 */

void printInBin(u64_t x)
{
    char str[64 + 8] = {'0'};
    u64_t n = x;
    char nibble = 8;
    size_t e = 0;

    for (int i = 0; i < 64; i++)
    {
        if (nibble == 0 && (i + 1) < 64)
        {
            str[e] = '.';
            nibble = 8;
            e++;
        }
        str[e] = ((n & 1) ? '1' : '0');
        n >>= 1;
        nibble--;
        e++;
    }
    str[64 + 7] = '\0';
    reverse(str, 64 + 7);
    kprintf("\n%s", str);
}

// function to convert decimal to octal
void printInOctal(u64_t n)
{
    // array to store octal number
    u64_t octalNum[100];
    kprintf("\n");
    // counter for octal number array
    u64_t i = 0;
    while (n != 0)
    {
        // storing remainder in octal array
        octalNum[i] = n % 8;
        n = n / 8;
        i++;
    }

    // printing octal number array in reverse order
    for (int j = i - 1; j >= 0; j--)
    {
        kprintf("%d", octalNum[j]);
    }
}
/**
 * @brief Exibe um endereço vitual em binário, com todos os seus
 * campos de índice e offset devidamente demarcados. Uso para
 * debug.
 *
 * @param address
 */

void printVirtualAddressInBin(u64_t address)
{
    int i = 0;

    kprintf("\n");

    for (i = (sizeof(u64_t) * 8); i > 0; i--)
    {

        if (i == 12 || i == 48)
        {
            kprintf(">.<");
        }
        else if (i == 21 || i == 30 || i == 39)
        {
            kprintf(">.<");
        }
        kprintf("%c", (address & ((u64_t)1 << (i - 1)) ? '1' : '0'));
    }
}

void printVirtualAddressInOct(u64_t n)
{
    // array to store octal number
    u64_t octalNum[100];

    kprintf("\n");
    // counter for octal number array
    u64_t i = 0;
    while (n != 0)
    {
        // storing remainder in octal array
        octalNum[i] = n % 8;
        n = n / 8;
        i++;
    }

    // printing octal number array in reverse order
    int c = 0;
    kprintf("<");

    for (int j = i - 1; j >= 0; j--)
    {

        if (c == 6 || c == 9 || c == 12 || c == 15 || c == 18)
        {
            kprintf(">.<");
        }
        kprintf("%d", octalNum[j]);
        c++;
    }
    kprintf(">");
}

void print_bin(long long int intNum)
{
    print_int(intNum, BASE_BIN);
}
void print_dec(long long int intNum)
{
    print_int(intNum, BASE_DEC);
}
void print_hex(long long int intNum)
{
    print_int(intNum, BASE_HEX);
}

static void print_int(int64_t intNum, int base)
{
    char str[64 + 1];
    itoa(intNum, str, base);
    puts(str);
    puts("\n");
}

/**
 * @brief Imprime os valores constantes dos registro do processador
 *
 */
void dump_registros()
{
    u64_t regs[17] = {0};
    static char *str_regs[] = {"(RAX)", "(RBX)", "(RCX)", "(RDX)", "(RDI)", "(RSI)",
                               "(RBP)", "(RSP)", "(R08)", "(R09)", "(R10)", "(R11)",
                               "(R12)", "(R13)", "(R14)", "(R15)", "(RIP)"};

    __dump_regs(regs); // rotina em assembly que preenche a matriz regs.

    /*
    regs[REG_RAX] = __dump_rax();
    regs[REG_RIP] = __dump_rip();
    regs[REG_RBX] = __dump_rbx();
    regs[REG_RCX] = __dump_rcx();
    regs[REG_RDX] = __dump_rdx();
    regs[REG_RDI] = __dump_rdi();
    regs[REG_RSI] = __dump_rsi();
    regs[REG_RBP] = __dump_rbp();
    regs[REG_RSP] = __dump_rsp();
    regs[REG_R8] = __dump_r8();
    regs[REG_R9] = __dump_r9();
    regs[REG_R10] = __dump_r10();
    regs[REG_R11] = __dump_r11();
    regs[REG_R12] = __dump_r12();
    regs[REG_R13] = __dump_r13();
    regs[REG_R14] = __dump_r14();
    regs[REG_R15] = __dump_r15();
    */

    for (int i = 0; i < 17; i++)
    {
        kprintf("\n   %s = %x", str_regs[i], regs[i]);
    }
}
