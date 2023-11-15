/*--------------------------------------------------------------------------
*  File name:  tsc.h
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 27-08-2023
*--------------------------------------------------------------------------
Este header reune
--------------------------------------------------------------------------*/
#pragma once

#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"
#include "kcpuid.h"
#include "x86_64.h"

#define CPUID_TSC 0x80000007

#define CPUID_PPM_ECX_TSC_COUNTER_RATIO BIT(0)

#define CPUID_APMF_EDX_TS BIT(0)
#define CPUID_APMF_EDX_FID BIT(1)
#define CPUID_APMF_EDX_VID BIT(2)
#define CPUID_APMF_EDX_TTP BIT(3)
#define CPUID_APMF_EDX_TM BIT(4)
#define CPUID_APMF_EDX_100MHS BIT(6)
#define CPUID_APMF_EDX_HWPSTATE BIT(7)
#define CPUID_APMF_EDX_TSC_INVARIANT BIT(8)
#define CPUID_APMF_EDX_CPB BIT(9)
#define CPUID_APMF_EDX_EFFECTIVE_FREQUENCY BIT(10)
#define CPUID_APMF_EDX_FEEDBACK BIT(11)
#define CPUID_APMF_EDX_PPREPOR BIT(12)

#define CPUID_TSC_EDX_RDTSCP BIT(27)

u64_t hpet_calibrate_tsc(void);
u64_t TSC_periodo(void);
u64_t TSCP_periodo(void);
void setup_cpu_tsc(void);

static inline bool is_tsc_invariant(void)
{
    cpuid_regs_t cpuid_ret;
    cpuid_ret = cpuid_get(CPUID_TSC);
    return (cpuid_ret.edx & CPUID_APMF_EDX_TSC_INVARIANT);
}
static inline bool is_tsc_present(void)
{
    cpuid_regs_t cpuid_ret;
    cpuid_ret = cpuid_get(CPUID_GETFEATURES);
    return (cpuid_ret.edx & CPUID_FEAT_EDX_TSC);
}
static inline bool has_rdtscp_suport(void)
{
    cpuid_regs_t cpuid_ret;
    cpuid_ret = cpuid_get(CPUID_GETFEATURES);
    return (cpuid_ret.edx & CPUID_TSC_EDX_RDTSCP);
}
static inline u64_t tsc_counter_read(void)
{
    return __read_tsc();
}

/* Stop speculative execution */
static inline void sync_core(void)
{
    int tmp;
    asm volatile("cpuid"
                 : "=a"(tmp)
                 : "0"(1)
                 : "ebx", "ecx", "edx", "memory");
}

static inline u64_t tsc_read(void)
{
    u64_t tsc;
    asm volatile("lfence; rdtsc; lfence;" // serializing read of tsc
                 "shl $32,%%rdx; "        // shift higher 32 bits stored in rdx up
                 "or %%rdx,%%rax"         // and or onto rax
                 : "=a"(tsc)              // output to tsc variable
                 :
                 : "%rcx", "%rdx", "memory"); // rcx and rdx are clobbered
    return tsc;
}