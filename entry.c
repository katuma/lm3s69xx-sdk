// glue code to make CS3 ANSI C library working

#include <sys/stat.h>

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

// int00 - this is the actual entrypoint
extern void __cs3_start_c(void);
void __cs3_reset() {
    // enable uart0 as stdout
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    IntMasterEnable();
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    return __cs3_start_c();
}

int _write(int file, char *ptr, int len) {
    while (len--)
        UARTCharPut(UART0_BASE, *ptr++);
}

int _read(int file, char *ptr, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        char c = ptr[i] = UARTCharGet(UART0_BASE);
// XXX horrible, ugly console echo hack
#if 1
        if (c == 10) UARTCharPut(UART0_BASE, 13);
        UARTCharPut(UART0_BASE, c);
        if (c == 13) UARTCharPut(UART0_BASE, c = ptr[i] = 10);
#endif
        if (c == 10) { i++; break; }
    }
    return i;
}

int _open(const char *name, int flags, int mode) {
    return -1;
}

int _lseek(int file, int ptr, int dir) {
    return 0;
}

int _isatty(int file) {
    return 1;
}

int _fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int _close(int file) {
    return 0;
}

char *heap_end = 0;
caddr_t _sbrk(int incr) {
    extern char __cs3_heap_start;
    extern char __cs3_heap_end;

    static int inself = 0;
    char *prev;
    if (!heap_end)
        heap_end = &__cs3_heap_start;
    prev = heap_end;
    if (heap_end + incr > &__cs3_heap_end) {
        write(1, "heap oflow\n", 11);
        return 0;
    }
    heap_end += incr;
    return prev;
}

