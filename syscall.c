//////////////////////////////////////////////////////////////////////////
// newlibc syscalls
//////////////////////////////////////////////////////////////////////////
#include "lwip/opt.h"

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

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

#pragma GCC visibility push(default)

int _write(int file, char *ptr, int len) {
    int i;
    for (i = 0; i < len; i++)
        UARTCharPut(UART0_BASE, ptr[i]);
    return i;
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

caddr_t _sbrk(int incr) {
    extern char __cs3_heap_start;
    extern char __cs3_heap_end;
    static char *heap_end = 0;

    char *prev;
    if (!heap_end)
        heap_end = &__cs3_heap_start;
    prev = heap_end;
    if (heap_end + incr > &__cs3_heap_end) {
        //write(1, "heap oflow\n", 11);
        return 0;
    }
    heap_end += incr;
    return prev;
}

int _exit(int code)
{
    // this will kill qemu & hopefully spew exit code
    HWREG(0xE000EF00 + code) = code;
}

#pragma GCC visibility pop
