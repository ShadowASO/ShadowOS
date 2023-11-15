/*--------------------------------------------------------------------------
*  File name:  keyboard.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 07-11-2020
*  Data revisão: 19-05-2022
*--------------------------------------------------------------------------
Este arquivo fonte possui o handler das interrupções de teclado e diversos
recursos relacionados com a manipulação das entradas do teclado.
--------------------------------------------------------------------------*/
#include <stdint.h>
#include "keyboard.h"
#include "io.h"
#include "screen.h"
#include "stdio.h"
#include "isr.h"
#include "x86_64.h"
#include "abnt2.h"
#include "ascii.h"
#include "lapic.h"
#include "sync/mutex.h"
#include "interrupt.h"

static uint8_t capslock = 0;
static uint8_t numblock = 0;
static uint8_t shift = 0;
static uint8_t ctrl = 0;
static uint8_t flag_numlock = 0;
static uint8_t flag_escape_E0 = 0;

static uint8_t kb_buffer[KEYBOARD_BUFFER_SIZE] = {0}; // 256
static uint8_t kb_write = 0;
static uint8_t kb_read = 0;

static uint8_t kb_char = 0;

// Vetor com teclas do teclado
static scvk_t keymap[KEYMAP_SIZE] = {0}; // 256
// Crio a variável que funcionará como mutex
CREATE_SPINLOCK(spinlock_key);

uint8_t get_keyboard_status(void)
{
    unsigned char status = 0;
    status = __read_portb(KEYBOARD_CTRL); // 0x64
    return status;
}
void clear_keyboard_buffer(void)
{
    unsigned char status = get_keyboard_status(); // 0x64
    while ((status & STATUS_BUFFER_READ_FULL))
    {
        __read_portb(KEYBOARD_DATA);          // 0x60
        status = __read_portb(KEYBOARD_CTRL); // 0x64
    }
}

void setup_keyboard()
{
    static uint8_t flag_keyboard_ini = 0;
    if (!flag_keyboard_ini)
    {
        keyboard_abnt2(keymap);
        flag_keyboard_ini = 1;
    }
    //  Limpo o buffer do teclado, ou então haverá muitos problemas por conta do lixo
    clear_keyboard_buffer();

    // irq_umask(ISR_VECTOR_KEYBOARD, cpu_id());
    add_handler_irq(ISR_VECTOR_KEYBOARD, keyboard_handler);
}
/**
 * @brief Faz a leitura de uma entrada bufferizada.
 *
 * @return uint8_t
 */
uint8_t kgetch(void)
{
    uint8_t result = 0;

    // spinlock_lock(&spinlock_key);
    irq_umask(ISR_VECTOR_KEYBOARD, cpu_id());

    while (result == 0)
    {

        if (kb_read < kb_write)
        {
            // mutex_spinlock(&mutex_key);

            result = kgetc(kb_buffer[kb_read]);
            kb_read = (kb_read + 1) % KEYBOARD_BUFFER_SIZE;

            // mutex_unlock(&mutex_key);
        }

        else if (kb_read == kb_write)
        {
            kb_write = 0;
            kb_read = 0;
        }

        if (result == KEY_ENTER)
        {
            kb_write = 0;
            kb_read = 0;
            break;
        }
        if (result == KEY_UP)
        {
            kb_write = 0;
            kb_read = 0;
            break;
        }
        if (result == KEY_DOWN)
        {
            kb_write = 0;
            kb_read = 0;
            break;
        }
        /* Este loop Estava com um consumo absurdo dos recursos da CPU.
        com a utilização desses comandos assembly inline, conseguir re-
        duzir. Essas macros inserem um código assembly 'pause' e 'hlt'.
        */
        __PAUSE__();
        __HLT__();
    }
    // spinlock_unlock(&spinlock_key);
    irq_mask(ISR_VECTOR_KEYBOARD, cpu_id());
    return result;
}
/**
 * @brief Converte um keycode do teclado em um caractere ASCII
 *
 * @param keycode
 * @return uint8_t
 */

uint8_t kgetc(uint8_t keycode)
{
    uint8_t result = 0;

    // Detect if shift is pressed
    if (keycode == SC_LSHIFT)
    {
        shift |= 1;
    }
    else if (keycode == SC_RSHIFT)
    {
        shift |= 2;
    }
    else if (keycode == (SC_LSHIFT | 0x80))
    {
        shift &= ~1;
    }
    else if (keycode == (SC_RSHIFT | 0x80))
    {
        shift &= ~2;
    }
    else if (keycode == SC_CAPSLOCK)
    {
        capslock = (capslock ^ 1);
    }
    else if (keycode == SC_NUMLOCK)
    {
        flag_numlock = (flag_numlock ^ 1);
    }
    else if (keycode == KBC_ESCAPE_E0)
    {
        flag_escape_E0 = 1;
    }
    else if (flag_escape_E0 && (KBC_ESCAPE_E0 != keycode))
    {
        result = get_tochar_with_escape(keycode);
    }
    else
    {
        result = get_tochar(keycode);
    }

    return result;
}
/**
 * @brief Faz a leitura diretamente do teclado, sem a necessidade de interrupções.
 *
 * @return uint8_t
 */

uint8_t kget_key(void)
{
    unsigned char status = 0;
    uint8_t keycode = 0;
    uint8_t result = 0;
    kb_char = 0; // Éimportantissimo zerar essa variável statica para que ela
    // não guarde o último caractere.

    status = __read_portb(KEYBOARD_CTRL); // 0x64
    while (!(status & 1))
    {
        __PAUSE__();
        __HLT__();
        status = __read_portb(KEYBOARD_CTRL); // 0x64
    }

    if ((status & 1))
    {
        keycode = __read_portb(KEYBOARD_DATA); // 0x60
        uint8_t vk = keymap[keycode].vk;
        // kprintf("\nkeycode=%x - vk=%x - flag_escape_E0=%x", keycode, vk, flag_escape_E0);

        // Detect if shift is pressed
        if (keycode == SC_LSHIFT)
        {
            shift |= 1;
        }
        else if (keycode == SC_RSHIFT)
        {
            shift |= 2;
        }
        else if (keycode == (SC_LSHIFT | 0x80))
        {
            shift &= ~1;
        }
        else if (keycode == (SC_RSHIFT | 0x80))
        {
            shift &= ~2;
        }
        else if (keycode == SC_CAPSLOCK)
        {
            capslock = (capslock ^ 1);
        }
        else if (keycode == SC_NUMLOCK)
        {
            flag_numlock = (flag_numlock ^ 1);
        }
        else if (keycode == KBC_ESCAPE_E0)
        {
            flag_escape_E0 = 1;
        }
        else if (flag_escape_E0 && (KBC_ESCAPE_E0 != keycode))
        {
            result = get_tochar_with_escape(keycode);
        }
        else
        {
            result = get_tochar(keycode);
        }
    }

    return result;
}

void keyboard_handler(cpu_regs_t *tsk_contxt)
{
    unsigned char status = 0;
    uint8_t keycode = 0;

    // Semáforo
    // mutex_spinlock(&mutex_key);

    status = __read_portb(KEYBOARD_CTRL); // 0x64

    if (status & 1)
    {

        while (status & 1)
        {
            keycode = __read_portb(KEYBOARD_DATA); // 0x60
            kb_buffer[kb_write] = keycode;
            kb_write = (kb_write + 1) % KEYBOARD_BUFFER_SIZE;

            // kprintf("\n=>keycode->hex=%x -- dec=%d ** -- kb_write=%d",
            //         keycode, keycode, kb_write);

            status = __read_portb(KEYBOARD_CTRL); // 0x64
        }
    }

    // mutex_unlock(&mutex_key);
}

/**
 * Manipula os scan codes especiais, precedidos de E0
 */
uint8_t get_tochar_with_escape(uint8_t b)
{
    flag_escape_E0 = (flag_escape_E0 ^ 1);

    switch (b)
    {
    case SCE_DIVIDE:
        return '/';
        break;
    case SC_RETURN:
        return KEY_ENTER;
        break;
    case SCE_END:
        return KEY_END;
        break;
    case SCE_HOME:
        return KEY_HOME;
        break;
    case SC_DELETE:
        return KEY_DELETE;
        break;
    case SC_PAD_LEFT_4:
        return KEY_LEFT;
        break;
    case SC_PAD_UP_8:
        return KEY_UP;
        break;
    case SC_PAD_DOWN_2:
        return KEY_DOWN;
        break;
    case SC_PAD_RIGHT_6:
        return KEY_RIGHT;
        break;
    default:
        break;
    }

    return 0;
}
/*
 * Method that converts scan code to character
 */

uint8_t get_tochar(uint8_t b)
{
    uint8_t vk = keymap[b].vk;

    // kprintf("\nkeycode=%x - vk=%x", b, vk);

    if (b == SC_SPACE)

        return KEY_SPACE;

    if (b == SC_RETURN)

        return KEY_ENTER;

    if (b == SC_BACKSPACE)
    {
        return KEY_BACKSPACE;
    }

    // Letras do alfabeto
    if (((b >= SC_Q) && (b <= SC_P)) ||
        ((b >= SC_A) && (b <= SC_L)) ||
        ((b >= SC_Z) && (b <= SC_M)))
    {

        if (shift ^ capslock)
            return keymap[b].shift;
        else
            return keymap[b].vk;
    }

    // Teclado numérico
    if ((b >= SC_PAD_HOME_7) && (b <= SC_DELETE))
    {

        if (flag_numlock)
        {
            switch (b)
            {
            case SC_PAD_INSERT_0:
                return '0';
                break;
            case SC_PAD_END_1:
                return '1';
                break;
            case SC_PAD_DOWN_2:
                return '2';
                break;
            case SC_PAD_NEXT_3:
                return '3';
                break;
            case SC_PAD_LEFT_4:
                return '4';
                break;
            case SC_PAD_CLEAR_5:
                return '5';
                break;
            case SC_PAD_RIGHT_6:
                return '6';
                break;
            case SC_PAD_HOME_7: // 7
                return '7';
                break;
            case SC_PAD_UP_8: // 8
                return '8';
                break;
            case SC_PAD_PRIOR_9: // 9
                return '9';
                break;
            default:
                break;
            }
        }
    }

    if (b == SC_SUBTRACT)
    {

        if (shift ^ capslock)
            return keymap[b].shift;
        else
            return keymap[b].vk;
    }
    else if (b == SC_ADD)
    {

        if (shift ^ capslock)
            return keymap[b].shift;
        else
            return keymap[b].vk;
    }
    else if (b == SC_DELETE)
    {
        return ',';
    }
    else if (b == SC_MULTIPLY)
    {
        return '*';
    }
    else if (b == SC_ABNT_C1 || b == SC_ABNT_C2)
    {

        if (shift ^ capslock)
            return keymap[b].shift;
        else
            return keymap[b].vk;
    }

    else if (b == SC_OEM_1 || b == SC_OEM_2 || b == SC_OEM_3 || b == SC_OEM_4 ||
             b == SC_OEM_5 || b == SC_OEM_6 || b == SC_OEM_7 ||
             b == SC_OEM_COMMA || b == SC_OEM_PERIOD || b == SC_OEM_102 ||
             b == SC_OEM_MINUS || SC_OEM_PLUS)
    {

        if (shift ^ capslock)
            return keymap[b].shift;
        else
            return keymap[b].vk;
    }

    else if ((b >= SC_1) && (b <= SC_0))
    {
        if (shift ^ capslock)
            return keymap[b].shift;
        else
            return keymap[b].vk;
    }

    // Solta a tecla
    if (b & (1 << 7))
        return 0;
    if (SC_RSHIFT)
        return 0;
    return keymap[b].vk;
}
