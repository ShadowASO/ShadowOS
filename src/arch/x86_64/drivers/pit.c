/*--------------------------------------------------------------------------
*  File name:  pit.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 03-07-2021
*--------------------------------------------------------------------------
Este arquivo fonte possui diversos recursos relacionados com a tabela
IDT e seus descritores, necessários para a manipulação das interrupções.
--------------------------------------------------------------------------*/
#include <stdint.h>
#include "pit.h"
#include "io.h"
#include "x86_64.h"
#include "lapic.h"
#include "sync/spin.h"
#include "interrupt.h"

/* Interface to 8254 Programmable Interrupt Timer (PIT).
   Refer to [8254] for details. */

/* 8254 registers. */
#define PIT_PORT_CONTROL 0x43                        /* Control port. */
#define PIT_PORT_COUNTER(CHANNEL) (0x40 + (CHANNEL)) /* Counter port. */

/* PIT cycles per second. */
#define PIT_HZ 1193180
#define PIT_HZ_MS 1193

struct _pit pit_com;

// Variável a ser utilizada como mutex
CREATE_SPINLOCK(spinlock_pit);

/* Configure the given CHANNEL in the PIT.  In a PC, the PIT's
   three output channels are hooked up like this:

     - Channel 0 is connected to interrupt line 0, so that it can
       be used as a periodic timer interrupt, as implemented in
       Pintos in devices/timer.c.

     - Channel 1 is used for dynamic RAM refresh (in older PCs).
       No good can come of messing with this.

     - Channel 2 is connected to the PC speaker, so that it can
       be used to play a tone, as implemented in Pintos in
       devices/speaker.c.

   MODE specifies the form of output:

     - Mode 2 is a periodic pulse: the channel's output is 1 for
       most of the period, but drops to 0 briefly toward the end
       of the period.  This is useful for hooking up to an
       interrupt controller to generate a periodic interrupt.

     - Mode 3 is a square wave: for the first half of the period
       it is 1, for the second half it is 0.  This is useful for
       generating a tone on a speaker.

     - Other modes are less useful.

   FREQUENCY is the number of periods per second, in Hz. */
void pit_configure_channel(int channel, int mode, int frequency)
{
  uint16_t count;

  /* Convert FREQUENCY to a PIT counter value.  The PIT has a
     clock that runs at PIT_HZ cycles per second.  We must
     translate FREQUENCY into a number of these cycles. */
  if (frequency < 19)
  {
    /* Frequency is too low: the quotient would overflow the
         16-bit counter.  Force it to 0, which the PIT treats as
         65536, the highest possible count.  This yields a 18.2
         Hz timer, approximately. */
    count = 0;
  }
  else if (frequency > PIT_HZ)
  {
    /* Frequency is too high: the quotient would underflow to
         0, which the PIT would interpret as 65536.  A count of 1
         is illegal in mode 2, so we force it to 2, which yields
         a 596.590 kHz timer, approximately.  (This timer rate is
         probably too fast to be useful anyhow.) */
    count = 2;
  }
  else
    count = (PIT_HZ + frequency / 2) / frequency;

  pit_com.cha = channel;
  pit_com.ope = mode;
  pit_com.acc = 3;
  pit_com.bcd = 0;
  void *pt = &pit_com;
  /* Configure the PIT mode and load its counters. */
  //__write_portb(PIT_PORT_CONTROL, (channel << 6) | 0x30 | (mode << 1));
  __write_portb(PIT_PORT_CONTROL, *(uint8_t *)pt);
  __write_portb(PIT_PORT_COUNTER(channel), count);
  __write_portb(PIT_PORT_COUNTER(channel), count >> 8);
}
/**
 * @brief  Faz o sistema aguardar o número de milissegundos indicados em "ms"
 * @note
 * @param  ms:
 * @retval None
 */
void pit_sleep_ms(u64_t ms)
{
  // spin_lock(&pit_lock.flag);
  spinlock_lock(&spinlock_pit);

  irq_umask(ISR_VECTOR_PIT_TIMER, 0);

  u64_t total_count = PIT_HZ_MS * ms;

  do
  {
    uint16_t count = min(total_count, 0xFFFFU);
    // kprintf("\ncount=%d", count);
    __write_portb(0x43, 0x30);
    __write_portb(0x40, count & 0xFF);
    __write_portb(0x40, count >> 8);

    do
    {
      __PAUSE__();
      __write_portb(0x43, 0xE2);
      // kprintf("\ntotal_count=%d", total_count);

    } while ((__read_portb(0x40) & (1 << 7)) == 0);
    total_count = total_count - (uint16_t)count;
    // kprintf("\ntotal_count=%u", total_count);

  } while ((total_count & ~0xFFFF) != 0);

  // irq_mask(IRQ_APIC_PIT_TIMER, 0);
  // spin_unlock(&pit_lock.key);
  spinlock_unlock(&spinlock_pit);
}

void set_pit_freq(uint32_t hz)
{
  pit_com.cha = ePIT_CHANNEL0;
  pit_com.ope = ePIT_MODE3_SQUARE;
  pit_com.acc = ePIT_ACCESS_LOHIBYTE;
  pit_com.bcd = 0;
  void *pt = &pit_com;

  pit_configure_channel(ePIT_CHANNEL0, ePIT_MODE3_SQUARE, hz);
}
void disable_pit(void)
{
  pit_configure_channel(ePIT_CHANNEL0, ePIT_MODE0_INT, 0);
}