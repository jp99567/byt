
obj-m := owtherm.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

PWD := $(shell pwd)
KERNEL_DIR := /home/j/workspace/bbb-kernel/bb-kernel/KERNEL

default:
	$(MAKE) ARCH=arm CROSS_COMPILE=$(CROSS) -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	$(MAKE) ARCH=arm CROSS_COMPILE=$(CROSS) -C $(KERNEL_DIR) M=$(PWD) clean
