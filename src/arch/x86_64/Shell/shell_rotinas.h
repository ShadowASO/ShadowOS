/*--------------------------------------------------------------------------
*  File name:  shell_rotinas.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 03-12-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui o código de um shell
--------------------------------------------------------------------------*/
#pragma once

#include "stdlib.h"
#include "stdio.h"
#include "keyboard.h"
#include "shell.h"
#include "../drivers/graphic/vesa.h"
#include "../drivers/graphic/console.h"
#include "../drivers/graphic/graphics.h"
#include "../drivers/graphic/pixmap.h"
#include "lapic.h"
#include "gdt.h"
#include "scheduler.h"
#include "ctype.h"
#include "syscall/syscalls.h"
#include "../drivers/smbios/smbios.h"
#include "cache.h"
#include "mm/vmalloc.h"
#include "btree.h"
#include "interrupt.h"
#include "../include/time.h"
#include "percpu.h"
#include "rtc.h"
#include "../drivers/time/tsc.h"
#include "time.h"
#include "timer.h"
#include "sysinfo.h"
#include "../drivers/IO/pci.h"
#include "../drivers/IO/ahci.h"
#include "io.h"
#include "smp.h"
#include "runq.h"
#include "kernel.h"
#include "../user/printf.h"
#include "../user/fork.h"
#include "../user/read.h"

static inline void shell_help(void);
static inline void shell_cls(void);
static inline void shell_video(void);
static inline void shell_console(void);
static inline void shell_smp(void);
static inline void shell_trampoline(void);
static inline void shell_mmlimits(void);
static inline void shell_mmsize(void);
u64_t __testa_segment_mem(void);

extern u32_t p4_table;
extern volatile u64_t label_trampoline;

virt_addr_t pt[100];
struct task *sys_task = NULL;
struct percpu *pcpu = NULL;
u8_t core_id = 0;

CREATE_SPINLOCK(spinlock_cpu);

extern struct graphics *g_obj1;
extern console_t *tty1;

/* Rotina de teste utilizada com threads com passagem
de argumentos. */
static int print_thread(void *argv)
{
    char ch = *(char *)argv;
    while (true)
    {
        printf("%c", ch);

        /* Desabilito a preempção.*/
        hpet_sleep_milli(100);

        /* Reabilito a preempção.*/
        sched_yield();
    }
    return 0;
}
static void user_show_cpu(void)
{
    char buf[10] = {0};
    char *fmt = "[%d]";
    char *p = buf;

    bool blink = true;

    int row = tty1->ysize - 1;
    int col = 0;
    uint32_t cpu = 0;
    uint32_t pid = 0;

    if (is_graphic_mode())
    {
        tty1->xpos = 0;
        tty1->ypos = 0;
    }

    while (true)
    {

        preempt_disable();

        cpu = getcpu(&cpu, 0);
        pid = getpid();

        col = pid * 4;

        p = parser_token(buf, fmt, cpu);

        if (blink)
            p = "[ ]";

        if (is_graphic_mode())
        {
            console_putstring_at(tty1, col, row, p);
        }
        else
        {
            print_at(p, row, col);
        }
        blink = (blink) ? false : true;

        //  hpet_sleep_milli(50);

        preempt_enable();

        yield();
    }
    // console_delete(tty1);
}

static void user_mode(void)
{
    char *msg = "\nUSERMODE - Printf=Pasandos params.";

    size_t r = 2;
    while (r)
    {
        printf(msg);
        r--;
    }

    while (true)
    {
        __PAUSE__();
    }
}

static const char *get_arg_pos(const char **argv, u8_t pos)
{
    const char *result = NULL;

    if (argv[pos] != NULL)
    {
        result = argv[pos];
    }
    return result;
}
static int kshell_execute(int argc, const char **argv)
{
    const char *cmd = argv[0];

    if (!strcmp(cmd, "help"))
    {
        shell_help();
        printf("\nRIP=%p", __read_rip());
    }
    else if (!strcmp(cmd, "cache"))
    {
        printf("\nCACHE CODE L1 - Size=%d kbytes", L1_code_cache_size());
        printf("\nCACHE CODE L1 - line size=%d bytes", L1_code_cache_line_size());
        printf("\nCACHE DATA L1 - Size=%d kbytes", L1_data_cache_size());
        printf("\nCACHE DATA L1 - line size=%d bytes", L1_data_cache_line_size());

        printf("\n\nCACHE L2 - Size=%d kbytes", L2_cache_size());
        printf("\nCACHE L2 - line size=%d bytes", L2_cache_line_size());
    }
    else if (!strcmp(cmd, "bitwise"))
    {
        u32_t v = 0xffffffff;
        u8_t e = 0x0f;
        print_bin(e & 0x0000000f);
    }
    else if (!strcmp(cmd, "cls"))
    {
        shell_cls();
    }
    else if (!strcmp(cmd, "console-teste"))
    {
        struct graphics *g = console_obj->gx;
        pixmap_t *pix = get_pixmap_root();

        printf("\nPIXMAP:  pix->width=%d - pix->height=%d - graphics_width(c->gx)=%d", pix->width, pix->height, graphics_width(console_obj->gx));
        printf("\nxpos=%d -  ypos=%d - xsize=%d - ysize=%d", console_obj->xpos, console_obj->ypos, console_obj->xsize, console_obj->ysize);
    }

    /***************************************************************************************/
    else if (!strcmp(cmd, "bitree"))
    {
        int vetor[10] = {2, 7, 9, 6, 3, 4, 45, 78, 23, 63};
        BITREE_CREATE(tree, compare_int);
        btree_node_t *new_node = NULL;

        for (int i = 0; i < 10; i++)
        {
            new_node = (btree_node_t *)kmalloc(sizeof(btree_node_t));
            if (new_node == NULL)
                continue;

            new_node->data = &vetor[i];
            new_node->left = NULL;
            new_node->right = NULL;

            bitree_insert(&tree, new_node);
        }

        printf("\n\n");
        bitree_list_tree_left(&tree, tree.root);
        printf("\n\n");
        bitree_list_tree_right(&tree, tree.root);
        btree_node_t *node = bitree_search(&tree, &vetor[4]);
        printf("\nnode=%p -> data=%d", node, *(int *)node->data);
    }
    /******************************************************************************************/
    else if (!strcmp(cmd, "layout"))
    {
        show_memory_physic_layout_info();
    }
    else if (!strcmp(cmd, "node"))
    {
        show_struct_node();
    }
    else if (!strcmp(cmd, "tsc-info"))
    {
        cpuid_regs_t cpuid_ret;
        cpuid_ret = cpuid_get(CPUID_TSC);

        printf("\ncpuid.edx=%x", cpuid_ret.edx);
        printf("\ncpuid.ecx=%x", cpuid_ret.ecx);

        if (is_tsc_invariant())
        {
            WARN_ENTER("TSC invariant.");
            printf("\ncpuid.edx=%x", cpuid_ret);
        }
    }
    else if (!strcmp(cmd, "ioapic-dump"))
    {
        dump_ioapic();
    }
    else if (!strcmp(cmd, "task"))
    {
        u8_t cpu = cpu_id();
        tss_t *tss = percepu_get_tss(cpu);
        task_t *tsk = percpu_current();

        printf("\ncpu=%d: task_curr=%p ->&cpu=%p =>rsp0=%p", cpu, tsk, &cpu, tsk->rsp0);
        printf("\ntask_curr->stack_rbp=%p -> task_curr->stack_rsp=%p", tsk->stack_rbp, tsk->stack_rsp);
    }

    else if (!strcmp(cmd, "video"))
    {
        // shell_video();
        refresh_screen_buffer();
    }
    else if (!strcmp(cmd, "console"))
    {
        shell_console();
    }
    else if (!strcmp(cmd, "pci"))
    {
        pci_probe();
        PCIAddress addr = pci_find_device(3, 0);
        show_device(addr);
    }
    else if (!strcmp(cmd, "fork"))
    {
        task_t *t = (task_t *)fork(user_show_cpu, 0, MODE_USER);
        core_id = execve(t, 0, 0);
    }
    else if (!strcmp(cmd, "execve"))
    {
        const char *c = get_arg_pos(argv, 1);
        cpuid_t core = 0;

        if (argc == 1)
        {
            printf("\nERROR: command execve [n] - informe um numero.");
            return 1;
        }
        u8_t n = stoi(c);

        for (size_t i = n; i > 0; i--)
        {
            struct task *t = (task_t *)fork(user_show_cpu, 0, MODE_USER);
            core = execve(t, 0, 0);
        }
    }
    else if (!strcmp(cmd, "kill"))
    {
        const char *c = get_arg_pos(argv, 1);
        cpuid_t core = 0;

        if (argc == 1)
        {
            printf("\nERROR: command kill [n] - informe o pid.");
            return 1;
        }
        u8_t n = stoi(c);
        kill(n, 0);
    }
    /* Rotina que imprime a n letras do alfabeto. */
    else if (!strcmp(cmd, "thread"))
    {
        const char *c = get_arg_pos(argv, 1);
        if (argc == 1)
        {
            printf("\nERROR: informe um 'nr'.");
            return 1;
        }

        u8_t n = stoi(c);
        char *ch = kmalloc(n + 1);
        ch[n + 1] = NULL_CHAR;

        for (int i = n; i > 0; i--)
        {
            ch[i] = 'A' + i;

            task_t *t = pthread_create(print_thread, &ch[i], THREAD_KERNEL | MODE_KERNEL);
            sched_execve(t);
            WARN_ON("task_new=%p", t);
        }
    }
    else if (!strcmp(cmd, "ps"))
    {
        show_task_running();
    }
    else if (!strcmp(cmd, "printf"))
    {
        char buf[124] = {0};
        sprintf(buf, "\nTestando sys_write %d - %x", 34, 34);
        printf("\nResultado: %s", buf);

        printf("\nTestando sys_write %d - %x", 34, 34);
    }
    else if (!strcmp(cmd, "smp"))
    {
        printf("\ncurrent core=%d", cpu_id());
    }
    else if (!strcmp(cmd, "smbios"))
    {
        find_smbios_log();
    }

    else if (!strcmp(cmd, "trampoline"))
    {
        shell_trampoline();
    }
    else if (!strcmp(cmd, "init-mm"))
    {
        show_mm_struct(&init_mm);
    }
    else if (!strcmp(cmd, "mm-mapa"))
    {
        print_memory_map();
        dbg_show_variaveis();
    }
    else if (!strcmp(cmd, "show-tables"))
    {
        show_pagetables_alloc_statistic();
    }

    else if (!strcmp(cmd, "mm-limits"))
    {
        shell_mmlimits();
    }

    else if (!strcmp(cmd, "vmalloc"))
    {
        virt_addr_t vp = NULL;
        size_t size = (PAGE_SIZE * 3);
        static u8_t i = 0;
        static u8_t x = 0;

        x = i + 3;

        for (; i < x; i++)
        {
            pt[i] = vmalloc(size);
            printf("\nVMALLOC: %d - size=%d -->%p ", i, size, pt[i]);
            memset(vp, 0, size);
        }
    }
    else if (!strcmp(cmd, "vfree"))
    {

        if (argv[1] != 0)
        {
            int ini = stoi(argv[1]);
            vfree(pt[ini]);
            pt[ini] = NULL;
            for (int i = 0; i < 10; i++)
            {
                printf("\nvfree: %d -->%p ", i, pt[i]);
            }
        }
    }

    else if (!strcmp(cmd, "vm-lists"))
    {
        show_vmalloc_used_lists();
    }
    else if (!strcmp(cmd, "kmalloc"))
    {
        static size_t size = 0;
        static u8_t i = 0;
        static u8_t l = 0;

        size = 0x1000 - 2;
        l = i + 10;
        for (; i < l; i++)
        {
            pt[i] = kmalloc(size);
            printf("\nkmalloc: %d - size=%d -->%p ", i, size, pt[i]);
        }
    }
    else if (!strcmp(cmd, "kfree"))
    {

        if (argv[1] != 0)
        {
            int ini = stoi(argv[1]);
            for (int x = ini; x < (ini + 10); x++)
            {
                printf("\nkfree: %d -->%p ", x, pt[x]);
                kfree(pt[x]);
                pt[x] = NULL;
            }
        }
    }
    else if (!strcmp(cmd, "brk"))
    {
        brk(MIN_BLOCK_SIZE);
    }
    else if (!strcmp(cmd, "debug"))
    {
        mm_addr_t *ptr = (mm_addr_t *)0xFFFFFFFF8014bb44;
        for (int i = 0; i < 10; i++)
        {
            printf("\n[%d]=%x", i, ptr[i]);
        }

        printf("\ntss_percpu=%p", tss_percpu);
        printf("\ngdt_percpu=%p", gdt_percpu);
        printf("\nidt_percpu=%p", idt_percpu);
    }
    else if (!strcmp(cmd, "timer"))
    {

        struct rtc_time rtc_time;
        rtc_time = get_rtc_time();
        printf("\rdia=%d", rtc_time.day);
        printf("\rmes=%d", rtc_time.mth);
        printf("\rano=%d", rtc_time.year);

        printf("\rseculo=%d", rtc_time.century);

        printf("\rhora=%d", rtc_time.hr);
        printf("\rminuto=%d", rtc_time.min);
        printf("\rsegundos=%d", rtc_time.sec);

        u64_t sec = mktime(rtc_time.year, rtc_time.mth, rtc_time.day, rtc_time.hr, rtc_time.min, rtc_time.sec);

        printf("\nSeconds from 1971=%d", sec);
    }
    else if (!strcmp(cmd, "hpet-info"))
    {
        u64_t volatile ini = 0;
        u64_t volatile end = 0;
        u64_t hpet_nano = 0;

        local_irq_disable();
        for (int i = 0; i < 5; i++)
        {
            __sync_lfence();
            ini = hpet_main_counter();

            hpet_sleep_nano(1000000);
            end = hpet_main_counter();

            hpet_nano = hpet_convert_ticks_to_nano(end - ini);

            printf("\n\n*************************************\n");
            printf("\n\nHPET: end=%x - ini=%x = %d ticks => nanos=%d", end, ini, (end - ini), hpet_nano);
        }
        local_irq_enable();
    }

    else if (!strcmp(cmd, "hpet-loops"))
    {
        u64_t volatile ini = 0;
        u64_t volatile end = 0;
        u64_t hpet_nano = 0;

        for (int i = 0; i < 7; i++)
        {
            local_irq_disable();

            ini = hpet_main_counter();
            for (size_t l = 0; l < LOOPS_COUNT; l++)
            {
            }
            end = hpet_main_counter();

            hpet_nano = hpet_convert_ticks_to_nano(end - ini);

            printf("\n\n*************************************\n");
            printf("\n\nHPET: end=%x - ini=%x = %d ticks", end, ini, (end - ini));
            printf("\nTime on nano=%d -> loops/nano=%d", hpet_nano, LOOPS_COUNT / (end - ini));

            local_irq_enable();
        }
        // printf("\nLoops periodo on femptosecond=%d", calibrate_loop_periodo_fs());
    }
    else if (!strcmp(cmd, "tsc-teste"))
    {
        u64_t volatile ini = 0;
        u64_t volatile end = 0;
        u64_t nano = 0;
        u64_t periodo = 0;

        periodo = TSC_periodo();

        local_irq_disable();

        __sync_mfence();
        __sync_lfence();

        for (int i = 0; i < 7; i++)
        {

            // ini = tsc_counter_read();
            ini = tsc_read();
            // end = tsc_counter_read();
            end = tsc_read();

            nano = ((end - ini) * periodo) / 1000000;

            printf("\n\n*************************************\n");
            printf("\n\nTSC: end=%x - ini=%x = %d ticks", end, ini, (end - ini));
            printf("\nTime on nano=%d", nano);
        }

        local_irq_enable();
    }

    else if (!strcmp(cmd, "timer-teste"))
    {
        static struct timer_list timer;
        timer.function = testa_timer;
        timer.data = (mm_addr_t)&timer;

        init_timer(&timer);
        mod_timer(&timer, get_jiffies() + 2000);
        add_timer(&timer);
    }
    else if (!strcmp(cmd, "buddy"))
    {
        show_heap_free_lists();
    }
    else if (!strcmp(cmd, "set-debug"))
    {
        set_debuging()
    }

    else if (!strcmp(cmd, "heap"))
    {
        show_heap_memory();
    }
    else if (!strcmp(cmd, "UTC"))
    {
        tm_t utc = timestamp_to_utc(xtime.tv_sec);
        kprintf("\n%d-%d-%d - %d:%d:%d", utc.tm_day, utc.tm_mon, utc.tm_year, utc.tm_hour, utc.tm_min, utc.tm_sec);
        kprintf("\ntimestamp=%d", mktime(rtc.year, rtc.mth, rtc.day, rtc.hr, rtc.min, rtc.sec));

        tm_t utc2 = {0};

        utc2.tm_year = 1968;
        utc2.tm_mon = 7;
        utc2.tm_day = 15;
        utc2.tm_hour = 18;
        utc2.tm_min = 0;
        utc2.tm_sec = 0;

        time_t time = utc_to_timestamp(utc2);
        tm_t utc3 = {0};

        kprintf("\n\n%d-%d-%d - %d:%d:%d", utc2.tm_day, utc2.tm_mon, utc2.tm_year, utc2.tm_hour, utc2.tm_min, utc2.tm_sec);
        kprintf("\ntimestamp=%d", time);
        utc3 = timestamp_to_utc(time);
        kprintf("\n%d-%d-%d - %d:%d:%d", utc3.tm_day, utc3.tm_mon, utc3.tm_year, utc3.tm_hour, utc3.tm_min, utc3.tm_sec);
    }
    else if (!strcmp(cmd, "types-size"))
    {
        show_types_size();
    }
    else if (!strcmp(cmd, "bootmem"))
    {
        show_struct_bootmem();
    }
    else if (!strcmp(cmd, "lapic"))
    {
        printf("\nlapic_desc[id].p_addr=%x", apic_system.phys_base);
        printf("\nlapic_desc[id].v_addr=%p", apic_system.virt_base);
    }
    // USO: lista 0 ... 3
    else if (!strcmp(cmd, "lista"))
    {

        printf("\n%s-%s", cmd, argv[1]);
        if (argv[1] != 0)
        {
            int p = stoi(argv[1]);
            print_lista_by_zone(p);
        }
    }

    else if (!strcmp(cmd, "zonas"))
    {
        zone_t *zone;

        for (int i = 0; i < pgdat.nr_zones; i++)
        {
            zone = zone_obj(i);
            zone->zone_free_pages.value = 0;
            printf("\nzone=%d - zone->zone_start_pfn=%d", i, zone->zone_start_pfn);
        }
    }
    else if (!strcmp(cmd, "mm-size"))
    {
        shell_mmsize();
        show_zone_free_lists();
    }
    else if (!strcmp(cmd, "mm-segmented"))
    {
        printf("\npercpu	= %x", __testa_segment_mem());
    }
    else if (!strcmp(cmd, "virtual"))
    {
        page_t *page = alloc_pages(GFP_ZONE_NORMAL, 0);

        printf("\nbck=%p", page);
        printf("\nbck->p4=%p", get_pgtable_vaddress(page, P1_ID));
        printf("\nbck->p4=%p", get_pagetable_address(page, P1_ID));
    }
    else
    {
        printf("\n%s: Comando incorreto ...", cmd);
    }
    return 0;
}

static inline void shell_help(void)
{
    printf("\ncls");
    printf("\nkernel");
    printf("\npci");
    printf("\nsmbios");
    printf("\nmm-mapa");
    printf("\nmm-limits");
    printf("\nshow-tables");
    printf("\nvmalloc");
    printf("\nvfree");
    printf("\nkmalloc");
    printf("\nbuddy");
    printf("\nheap");
    printf("\nlapic");
    printf("\nlista");
    printf("\nzonas");
    printf("\nmm-size");
    printf("\nvirtual");
    printf("\nhelp");
    printf("\nnode");
    printf("\ninit-mm");
    printf("\nquit");
}
static inline void shell_cls(void)
{
    if (is_graphic_mode())
    {
        console_clear_screen(console_obj);
    }
    else
    {
        clear_screen();
    }
}
static inline void shell_video(void)
{
    printf("\nAtributos do Modo de Vídeo:\n");

    printf("\n\tvideo_buffer=%x:\n", video_buffer);
    printf("\n\tvideo_physBase=%x:\n", video_physbase);
    printf("\n\tvideo_xbytes=%d:\n", video_xbytes);
    printf("\n\tvideo_xres=%d:\n", video_xres);
    printf("\n\tvideo_yres=%d:\n", video_yres);
    printf("\n\tbpp=%d", vbe_hmode.bpp);
}
static inline void shell_console(void)

{
    printf("\nAtributos do Console:\n");
    printf("\n\tconsole_obj->xpos=%x:\n", console_obj->xpos);
    printf("\n\tconsole_obj->ypos=%x:\n", console_obj->ypos);
}
static inline void shell_smp(void)

{

    printf("\nsmp_nr_cpus_ready=%d", smp_nr_cpus_ready);
    printf("\npercpu_init_ap_ok=%d", percpu_init_ap_ok);
    printf("\nsmp_ap_kmain_ok=%d", smp_ap_kmain_ok);
    printf("\nsmp_ap_started_flag=%d", smp_ap_started_flag);
    printf("\nsmp_ap_stack=%p", smp_ap_stack);
    printf("\nTRAMPOLINE_START=%x -->%p", TRAMPOLINE_START, phys_to_virt(TRAMPOLINE_START));
    printf("\nsmp_ap_kmain=%p", &smp_ap_kmain);
}
static inline void shell_trampoline(void)
{

    printf("\n\npercpu_init_ap_ok=%d", percpu_init_ap_ok);
    printf("\n\nsmp_ap_kmain_ok=%d", smp_ap_kmain_ok);
    printf("\n\nsmp_ap_started_flag=%d", smp_ap_started_flag);
    printf("\n\nsmp_ap_stack=%p", smp_ap_stack);
    printf("\n\nlabel_trampoline=%d", _GDT_TSS_SEL);
}
static inline void shell_mmlimits(void)

{

    printf("\nFim do kernel - virtual = %p", kernel_end_vm());
    printf("\nFim do kernel - fisico = %p", kernel_pend());
    printf("\nTamanho do kernel - fisico = %d bytes", kernel_size());
    printf("\n&_swapper_pt4_dir = %p", &_swapper_pt4_dir);
    printf("\n__read_cr3() = %x", __read_cr3());
    printf("\nsmp_ap_stack = %p", smp_ap_stack);
}
static inline void shell_mmsize(void)
{
    printf("\npage_t(sizeof(page_t))=%d bytes", sizeof(page_t));
    printf("\nzone_t(sizeof(zone_t))=%d bytes", sizeof(zone_t));
    printf("\nnode_t(sizeof(node_t))=%d bytes", sizeof(node_t));
    printf("\nmm_struct_t(sizeof(mm_struct_t))=%d bytes", sizeof(mm_struct_t));
    printf("\nvm_struct_t(sizeof(vm_struct_t))=%d bytes", sizeof(vm_struct_t));
}