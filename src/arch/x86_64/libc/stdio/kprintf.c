/*--------------------------------------------------------------------------
*  File name:  kprintf.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 10-08-2020
*--------------------------------------------------------------------------
Este arquivo de fonte possui diversos rotinas para impressão de strings
--------------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdbool.h>
#include "screen.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "../drivers/graphic/console.h"
#include "sync/spin.h"
#include "debug.h"
#include "percpu.h"
#include "kernel.h"

#define to_hex_char(x) ((x) < 10 ? (x) + '0' : (x)-10 + 'A')
#define ATOI_BUFLEN 128
#define KPRINTF_BUFFER 128

CREATE_SPINLOCK(kprintf_key);

static inline void kprintf_write_char(char c)
{

	if (is_graphic_mode())
	{
		console_putchar(console_obj, c);
	}
	else
	{
		putchar(c);
	}
}

static inline void kprintf_write_str(char *s)
{
	char *pt_c = s;

	if (is_graphic_mode())
	{
		console_putstring(console_obj, pt_c);
	}
	else
	{
		while (*pt_c)
		{
			{
				putchar(*pt_c);
			}
			pt_c++;
		}
	}
}

static inline char digit_to_char(unsigned int digit)
{

	if (digit < 16)
	{
		return "0123456789ABCDEF"[digit];
	}
	else
	{
		return '\0';
	}
}

static int atoi_print(u64_t num, bool sign, unsigned int base)
{

	char buf[ATOI_BUFLEN], *p_buf = buf + sizeof(buf) - 2;
	int nwritten = 0;
	buf[ATOI_BUFLEN - 1] = '\0';
	int64_t n = (int64_t)num;

	if (base == 16)
	{
		kprintf_write_char('0');
		kprintf_write_char('x');
	}

	if (num == 0)
	{
		kprintf_write_char('0');
		return 1;
	}
	else if (sign)
	{

		if (n < 0)
		{
			kprintf_write_char('-');
			nwritten++;
			n = -n;
		}
		while (n != 0)
		{
			*p_buf-- = digit_to_char(n % base);
			n /= base;
			nwritten++;
		}
	}

	else
	{
		while (n != 0)
		{
			*p_buf-- = digit_to_char(n % base);
			n /= base;
			nwritten++;
		}
	}
	// RSP_MAP();
	kprintf_write_str(p_buf + 1);

	return nwritten;
}

// Doesn't use locks. Calls must be serialised.
static int __kvprintf(const char *fmt, va_list params)
{
	size_t nwritten = 0;
	const char *pfmt = fmt;

	// RSP_MAP();

	while (*pfmt)
	{
		if (*pfmt == '%')
		{
			switch (pfmt[1])
			{
			case '%':
				kprintf_write_char('%');
				nwritten++;
				break;
			case 'c':
				kprintf_write_char((char)va_arg(params, int));
				nwritten++;
				break;
			case 's':
			{
				const char *s = va_arg(params, const char *);
				if (s == NULL)
					s = "(null)";
				kprintf_write_str((char *)s);
				nwritten += strlen(s);
				break;
			}
			case 'p':
			{
				char buf2[17], *p_buf2 = buf2 + 15;
				buf2[16] = '\0';
				uintptr_t ptr = (uintptr_t)va_arg(params, void *);
				while (p_buf2 >= buf2)
				{
					*p_buf2-- = to_hex_char(ptr & 0xF);
					ptr >>= 4;
				}
				kprintf_write_char('0');
				kprintf_write_char('x');
				kprintf_write_str(buf2);
				nwritten += 18;
				break;
			}
			case 'l':
				if (pfmt[2] == 'l')
					pfmt++;
				__attribute__((fallthrough));
			case 'z':
				if (pfmt[2] == 'd' || pfmt[2] == 'i')
					nwritten += atoi_print(va_arg(params, long), true, 10);
				else if (pfmt[2] == 'u')
					nwritten += atoi_print(va_arg(params, u64_t), false, 10);
				else if (pfmt[2] == 'x')
					nwritten += atoi_print(va_arg(params, u64_t), false, 16);
				pfmt++;
				break;
			case 'i':
			case 'b':
				nwritten += atoi_print(va_arg(params, int), false, 2);
				break;
			case 'd':
				nwritten += atoi_print(va_arg(params, int), true, 10);
				break;
			case 'u':
				// nwritten += atoi_print(va_arg(params, unsigned int), false, 10);
				nwritten += atoi_print(va_arg(params, unsigned int), false, 10);
				break;
			case 'x':
				nwritten += atoi_print(va_arg(params, unsigned int), false, 16);
				break;
			case 'o':
				// nwritten += atoi_print(va_arg(params, unsigned int), false, 8);
				nwritten += atoi_print(va_arg(params, u64_t), false, 8);
				break;
			default:
				// panic("kprintf: unknown format string character '%c'", pfmt[1]);
				break;
			}
			pfmt++;
			nwritten++;
		}
		else if (*pfmt == '\n')
		{
			kprintf_write_char('\n');
			nwritten++;
		}
		else if (*pfmt == '\t')
		{
			kprintf_write_char('\t');
			nwritten++;
		}
		else
			kprintf_write_char(*pfmt);
		pfmt++;
	}
	return nwritten;
}

int kprintf_lock(const char *fmt, ...)
{
	preempt_disable();

	va_list params;
	va_start(params, fmt);

	// bool locked = spin_try_lock(&kprintf_lock);
	spinlock_lock(&kprintf_key);

	int nwritten = __kvprintf(fmt, params);
	// if (locked) spin_unlock(&kprintf_lock);
	va_end(params);

	spinlock_lock(&kprintf_key);
	preempt_enable();

	return nwritten;
}

// Make sure not to call this from an NMI handler
int kprintf(const char *fmt, ...)
{
	va_list args;
	int nwritten = 0;
	char buf[KPRINTF_BUFFER];

	va_start(args, fmt);

	/* Desabilito a preempção no core atual. */
	// preempt_disable();

	// nwritten = __kvprintf(fmt, params);
	// nwritten = vsprintf(buf, fmt, params);
	nwritten = vscnprintf(buf, KPRINTF_BUFFER, fmt, args);
	kprintf_write_str(buf);

	/* Reabilito a preempção no core atual. */
	// preempt_enable();

	va_end(args);

	return nwritten;
}
