/*--------------------------------------------------------------------------
*  File name:  rtc.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 24-08-2023
*--------------------------------------------------------------------------
Este arquivo fonte possui diversas funções relacionadas ao gerenciamento do
tempo.
--------------------------------------------------------------------------*/
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"
#include "rtc.h"
#include "time.h"
#include "debug.h"
#include "stdio.h"
#include "string.h"
#include "io.h"
#include "info.h"

struct rtc_time last_rtc;

/* Verifica se uma atualização está em curso. */
bool is_rtc_update_in_progress(void)
{
    __write_portb(cmos_regs, 0x0A);
    return (__read_portb(cmos_data) & 0x80);
}

u8_t get_RTC_register(int reg)
{
    __write_portb(cmos_regs, reg);
    return __read_portb(cmos_data);
}
struct rtc_time get_rtc_time(void)
{
    read_rtc();
    return last_rtc;
}

void read_rtc(void)
{
    struct rtc_time first_rtc;

    /* Note: This uses the "read registers until you get the same values twice in a row" technique
           to avoid getting dodgy/inconsistent values due to RTC updates.*/

    while (is_rtc_update_in_progress())
        ;

    first_rtc.sec = get_RTC_register(RTC_REG_SEG);
    first_rtc.min = get_RTC_register(RTC_REG_MIN);
    first_rtc.hr = get_RTC_register(RTC_REG_HRS);
    first_rtc.day = get_RTC_register(RTC_REG_DIA);
    first_rtc.mth = get_RTC_register(RTC_REG_MES);
    first_rtc.year = get_RTC_register(RTC_REG_ANO);

    first_rtc.century = 0;
    if (hw_info.rtc_timer.century != 0)
    {
        first_rtc.century = get_RTC_register(hw_info.rtc_timer.century);
    }

    do
    {

        while (is_rtc_update_in_progress())
            ;

        last_rtc.sec = get_RTC_register(RTC_REG_SEG);
        last_rtc.min = get_RTC_register(RTC_REG_MIN);
        last_rtc.hr = get_RTC_register(RTC_REG_HRS);
        last_rtc.day = get_RTC_register(RTC_REG_DIA);
        last_rtc.mth = get_RTC_register(RTC_REG_MES);
        last_rtc.year = get_RTC_register(RTC_REG_ANO);

        last_rtc.century = 0;
        if (hw_info.rtc_timer.century != 0)
        {
            last_rtc.century = get_RTC_register(hw_info.rtc_timer.century);
        }

    } while ((first_rtc.sec != last_rtc.sec) || (first_rtc.min != last_rtc.min) || (first_rtc.hr != last_rtc.hr) ||
             (first_rtc.day != last_rtc.day) || (first_rtc.mth != last_rtc.mth) || (first_rtc.year != last_rtc.year) ||
             (first_rtc.century != last_rtc.century));

    /* Verifica se os valores estão no formato BCD e o convert to binary values.*/
    last_rtc.reg_b = get_RTC_register(RTC_SREG_B);
    if (!(last_rtc.reg_b & 0x04))
    {
        last_rtc.sec = (last_rtc.sec & 0x0F) + ((last_rtc.sec / 16) * 10);
        last_rtc.min = (last_rtc.min & 0x0F) + ((last_rtc.min / 16) * 10);
        last_rtc.hr = ((last_rtc.hr & 0x0F) + (((last_rtc.hr & 0x70) / 16) * 10)) | (last_rtc.hr & 0x80);
        last_rtc.day = (last_rtc.day & 0x0F) + ((last_rtc.day / 16) * 10);
        last_rtc.mth = (last_rtc.mth & 0x0F) + ((last_rtc.mth / 16) * 10);
        last_rtc.year = (last_rtc.year & 0x0F) + ((last_rtc.year / 16) * 10);

        if (hw_info.rtc_timer.century != 0)
        {
            last_rtc.century = (last_rtc.century & 0x0F) + ((last_rtc.century / 16) * 10);
        }
    }

    // Convert 12 hour clock to 24 hour clock if necessary

    if (!(last_rtc.reg_b & 0x02) && (last_rtc.hr & 0x80))
    {
        last_rtc.hr = ((last_rtc.hr & 0x7F) + 12) % 24;
    }

    // Calculate the full (4-digit) year

    if (hw_info.rtc_timer.century != 0)
    {
        last_rtc.year += last_rtc.century * 100;
    }
    else
    {
        last_rtc.year += (CURRENT_YEAR / 100) * 100;
        if (last_rtc.year < CURRENT_YEAR)
            last_rtc.year += 100;
    }
}