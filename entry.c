/*
 * Copyright (c) 2012-2013 Karel Tuma <karel.tuma@omsquare.com>
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 * 
 */

// entry point 
// peripheral init code
// interrupt vectors

#include "inc/hw_ints.h"
#include "inc/hw_ethernet.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/ethernet.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/uart.h"
#include "driverlib/systick.h"

#include "net.h"

#include <stdlib.h>
#include <stdio.h>

#define SERIAL_BAUD_RATE        115200
#define TICKS_PER_SECOND        100
#define SYSTICKHZ               TICKS_PER_SECOND
#define SYSTICKMS               (1000 / SYSTICKHZ)
#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xc0

static void sys_tick() {
    return net_timer(SYSTICKMS);
}

extern unsigned long __cs3_stack;
extern void __cs3_reset();

void general_fault()
{
    printf("general fault occured\n");
}

void (*const __cs3_interrupt_vector_custom[NUM_INTERRUPTS])(void) __attribute__ ((section (".cs3.interrupt_vector"))) =
{
    [0] = (void*)&__cs3_stack,
    [1] = __cs3_reset,
    [2] = general_fault,
    [3] = general_fault,
    [4] = general_fault,
    [5] = general_fault,
    [6] = general_fault,
    [7] = general_fault,
    [8] = general_fault,
    [9] = general_fault,
    [10] = general_fault,
    [11] = general_fault,
    [12] = general_fault,
    [13] = general_fault,
    [14] = general_fault,
    [FAULT_SYSTICK] = sys_tick,
    [INT_ETH] = net_inthandler,
};

// init all your peripherals here
extern void __cs3_start_c(void);
void __cs3_reset() {
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);

    // ticker
    SysTickPeriodSet(SysCtlClockGet() / SYSTICKHZ);
    SysTickEnable();
    SysTickIntEnable();

    // enable uart0 as stdout
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), SERIAL_BAUD_RATE, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // ethernet
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);
    SysCtlPeripheralReset(SYSCTL_PERIPH_ETH);
    IntPrioritySet(INT_ETH, ETHERNET_INT_PRIORITY);

    // enable interrupts
    IntMasterEnable();
    return __cs3_start_c(); // hands over control to main() eventually
}

#if DEBUG
void __error__(char *f, unsigned long l)
{
    printf("ti lib ASSERT @ %d:%s\n",l,f);
    exit(253);
}
#endif

