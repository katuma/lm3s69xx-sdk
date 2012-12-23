CC=~/CodeSourcery/Sourcery_CodeBench_Lite_for_ARM_EABI/bin/arm-none-eabi-gcc
OBJCOPY=~/CodeSourcery/Sourcery_CodeBench_Lite_for_ARM_EABI/bin/arm-none-eabi-objcopy
CFLAGS=-mthumb -mcpu=cortex-m3 -mfix-cortex-m3-ldrd -T lm3s6965.ld -I. -Dsourcerygxx=1
test1: test1.c
	$(CC) -Os $(CFLAGS) $< driverlib/*.c entry.c -o $@.o
	$(OBJCOPY) -O binary $@.o $@
clean:
	rm -f test1.o test1
test:
	qemu-system-arm -nographic -M lm3s6965evb --kernel test1 -icount 1
