/*******************************************************************************
 *
 * File: mouse.c
 *
 * Author: Alexy Torres Aurora Dugo
 *
 * Date: 16/12/2017
 *
 * Version: 1.5
 *
 * Mouse driver (PS2) for the kernel.
 ******************************************************************************/

// #include "../cpu/cpu.h"            /* outb inb */
// #include "../core/interrupts.h"    /* register_interrupt, cpu_state,
//*stack_state, set_IRQ_EOI, set_IRQ_mask * /
// #include "../lib/stdint.h"         /* Generic int types */
// #include "../lib/stddef.h"         /* OS_RETURN_E, OS_EVENT_ID */
// #include "../lib/string.h"         /* memcpy */
// #include "../sync/lock.h"          /* enable_local_interrupt, disable_local_interrupt */

/* Header include */
#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "mouse.h"
#include "ktypes.h"
#include "../sync/spin.h"
#include "isr.h"
#include "interrupt.h"
#include "lapic.h"
#include "io.h"

/*******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************/

/* Mouse state */
static mouse_state_t mouse_state;

/* Mouse state management */
static uint8_t mouse_cycle;
static int8_t mouse_byte[3];

/* Mouse lock */
// static lock_t mouse_events_lock;
CREATE_SPINLOCK(mouse_events_lock);

/* Events table */
static mouse_event_t mouse_events[MOUSE_MAX_EVENT_COUNT];

/*******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/* Induces a delay between two opperations on the mouse.
 * Allow to wait until A bit is 1 0r 0.
 *
 * @param a_type Tells if we should wait until A bit it so 1 or 0.
 */
__inline__ static void mouse_wait(const uint8_t a_type)
{
    uint32_t timeout = 100000;
    if (a_type != 0)
    {
        while (--timeout)
        {
            if ((__read_portb(MOUSE_COMM_PORT) & MOUSE_BBIT) == 1)
            {
                return;
            }
        }
        return;
    }
    else
    {
        while (--timeout)
        {
            if (!((__read_portb(MOUSE_COMM_PORT) & MOUSE_ABIT)))
            {
                return;
            }
        }
        return;
    }
}

/* Write to the mouse port.
 *
 * @param write The value to write to the data port
 */
__inline__ static void mouse_write(const uint8_t write)
{
    mouse_wait(1);
    __write_portb(MOUSE_WRITE, MOUSE_COMM_PORT);
    mouse_wait(1);
    __write_portb(write, MOUSE_DATA_PORT);
}

/* Read from the mouse port.
 *
 * @returns The value read from the mouse data port.
 */
__inline__ static uint8_t mouse_read(void)
{
    char t;
    mouse_wait(0);
    t = __read_portb(MOUSE_DATA_PORT);
    return t;
}

/* Mouse IRQ handler, read the mouse state and update the system mouse state.
 *
 * @param cpu_state The cpu registers before the interrupt.
 * @param int_id The interrupt line that called the handler.
 * @param stack_state The stack state before the interrupt.
 */
// static void mouse_interrupt_handler(cpu_state_t *cpu_state, uint32_t int_id,
//                                     stack_state_t *stack_state)
static void mouse_interrupt_handler(cpu_regs_t *tsk_contxt)
{
    //(void)cpu_state;
    //(void)stack_state;
    uint32_t vec_no = tsk_contxt->int_no & 0xFF;
    isr_obj_t *isr_obj = &isr_handlers[vec_no];

    int8_t mouse_in;
    uint8_t status;
    uint32_t i;

    // ISR_VECTOR_MOUSE
    /*
        if (int_id == MOUSE_INTERRUPT_LINE)
        {
            set_IRQ_EOI(MOUSE_IRQ_LINE);
        }
        */

    status = __read_portb(MOUSE_COMM_PORT);
    while (status & MOUSE_BBIT)
    {
        mouse_in = __read_portb(MOUSE_DATA_PORT);
        if (status & MOUSE_F_BIT)
        {
            switch (mouse_cycle)
            {
            case 0:
                mouse_byte[0] = mouse_in;
                if (!(mouse_in & MOUSE_V_BIT))
                {
                    return;
                }
                ++mouse_cycle;
                break;
            case 1:
                mouse_byte[1] = mouse_in;
                ++mouse_cycle;
                break;
            case 2:
                mouse_byte[2] = mouse_in;
                /* We now have a full mouse packet ready to use */
                if (mouse_byte[0] & 0x80 || mouse_byte[0] & 0x40)
                {
                    /* x/y overflow? bad packet! */
                    break;
                }

                if (mouse_byte[1] != 0 || mouse_byte[2] != 0)
                {
                    mouse_state.pos_x = mouse_byte[1];
                    mouse_state.pos_y = mouse_byte[2];
                }
                else
                {
                    mouse_state.pos_x = 0;
                    mouse_state.pos_y = 0;
                }

                /* Managing clicks */
                if (mouse_byte[0] & 0x01)
                {
                    mouse_state.flags |= MOUSE_LEFT_CLICK;
                }
                else if (mouse_state.flags & MOUSE_LEFT_CLICK)
                {
                    mouse_state.flags &= ~MOUSE_LEFT_CLICK;
                }
                if (mouse_byte[0] & 0x02)
                {
                    mouse_state.flags |= MOUSE_RIGHT_CLICK;
                }
                else if (mouse_state.flags & MOUSE_RIGHT_CLICK)
                {
                    mouse_state.flags &= ~MOUSE_RIGHT_CLICK;
                }
                if (mouse_byte[0] & 0x04)
                {
                    mouse_state.flags |= MOUSE_MIDDLE_CLICK;
                }
                else if (mouse_state.flags & MOUSE_MIDDLE_CLICK)
                {
                    mouse_state.flags &= ~MOUSE_MIDDLE_CLICK;
                }
            default:
                break;
            }
            mouse_cycle = 0;
            break;
        }
    }
    status = __read_portb(MOUSE_COMM_PORT);

    /* Execute events */
    for (i = 0; i < MOUSE_MAX_EVENT_COUNT; ++i)
    {
        if (mouse_events[i].enabled == 1)
        {
            mouse_events[i].execute();
        }
    }
}

// OS_RETURN_E init_mouse(void)
int init_mouse(void)
{
    // OS_RETURN_E err;
    uint32_t i;

    /* Init mouse setings */
    mouse_state.pos_x = 0;
    mouse_state.pos_y = 0;
    mouse_state.flags = 0;
    mouse_cycle = 0;

    spinlock_init(&mouse_events_lock);

    mouse_wait(1);
    __write_portb(0xA8, MOUSE_COMM_PORT);
    mouse_wait(1);
    __write_portb(0x20, MOUSE_COMM_PORT);
    mouse_wait(0);
    int8_t status = __read_portb(MOUSE_DATA_PORT) | 2;
    mouse_wait(1);
    __write_portb(0x60, MOUSE_COMM_PORT);
    mouse_wait(1);
    __write_portb(status, MOUSE_DATA_PORT);
    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();

    for (i = 0; i < MOUSE_MAX_EVENT_COUNT; ++i)
    {
        mouse_events[i].enabled = 0;
    }

    /* Set PS2 interrupt handler */
    // err = register_interrupt_handler(MOUSE_INTERRUPT_LINE,
    //                                  mouse_interrupt_handler);
    add_handler_irq(ISR_VECTOR_MOUSE, mouse_interrupt_handler);
    /*
        if (err != OS_NO_ERR)
        {
            return err;
        }
    */
    /* Set PS2 IRQ */
    // err = set_IRQ_mask(MOUSE_IRQ_LINE, 1);
    irq_mask(ISR_VECTOR_MOUSE, 0);

    /* Dummy read port to enable interrupt in case it's disabled */
    __read_portb(MOUSE_COMM_PORT);
    __read_portb(MOUSE_DATA_PORT);

    // return err;
    return 0;
}

// int register_mouse_event(void (*function)(void),
// OS_EVENT_ID *event_id)

int register_mouse_event(void (*function)(void), int event_id)
{
    uint32_t i;

    if (function == NULL)
    {
        if (event_id != 0)
        {
            event_id = -1;
        }
        return 0;
    }

    spinlock_lock(&mouse_events_lock);

    /* Search for free event id */
    for (i = 0; i < MOUSE_MAX_EVENT_COUNT && mouse_events[i].enabled == 1; ++i)
        ;

    if (i == MOUSE_MAX_EVENT_COUNT)
    {
        if (event_id != 0)
        {
            event_id = -1;
        }
        spinlock_unlock(&mouse_events_lock);
        return 0;
    }

    /* Create new event */
    mouse_events[i].execute = function;
    mouse_events[i].event_id = i;
    mouse_events[i].enabled = 1;

    if (event_id != 0)
    {
        event_id = i;
    }

#ifdef DEBUG_MOUSE
    kernel_serial_debug("Registered mouse event id %d, handler 0x%08x\n",
                        i, (uint32_t)function);
#endif

    spinlock_unlock(&mouse_events_lock);

    return 0;
}

int unregister_mouse_event(const int event_id)
{
    if (event_id >= MOUSE_MAX_EVENT_COUNT)
    {
        return 0;
    }

    spinlock_lock(&mouse_events_lock);

    if (mouse_events[event_id].enabled == 0)
    {
        spinlock_unlock(&mouse_events_lock);
        return 0;
    }

    /* Disable event */
    mouse_events[event_id].enabled = 0;
    mouse_events[event_id].execute = NULL;

#ifdef DEBUG_MOUSE
    kernel_serial_debug("Unregistered mouse event id %d\n", event_id);
#endif

    spinlock_unlock(&mouse_events_lock);

    return 0;
}

// OS_RETURN_E get_mouse_state(mouse_state_t *state)
void *get_mouse_state(mouse_state_t *state)
{
    if (state == NULL)
    {
        return NULL;
    }

    memcpy(state, &mouse_state, sizeof(mouse_state_t));

    return 0;
}
