CSS:=~/CodeSourcery/Sourcery_CodeBench_Lite_for_ARM_EABI
CC=$(CSS)/bin/arm-none-eabi-gcc
OBJCOPY=$(CSS)/bin/arm-none-eabi-objcopy
CFLAGS=-mthumb -mcpu=cortex-m3 -mfix-cortex-m3-ldrd -T lm3s6965.ld -I. -Ilwip/src/include -Ilwip/src/include/ipv4  -Ilwip/src/include/ipv6 -std=c99 -Dsourcerygxx=1 -Os
LIB=syscall.c entry.c arch.c net.c eth.c $(wildcard driverlib/*.c lwip/src/netif/*.c lwip/src/core/*.c lwip/src/core/ipv4/*.c)
all: lwip ex1 ex2
lwip:
	git clone git://git.savannah.nongnu.org/lwip.git
.c.o:
	$(CC) $(CFLAGS) $(LIB) $< -o $@
ex2: ex2.o
	$(OBJCOPY) -O binary $@.o $@
ex1: ex1.o
	$(OBJCOPY) -O binary $@.o $@
clean:
	rm -f ex1 ex2 *.o

