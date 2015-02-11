
obj-m := owtherm.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

PWD := $(shell pwd)
KERNEL_DIR := /home/j/beaglebone/workspace/linux-dev-3.8.13-bone50/KERNEL/

default:
	$(MAKE) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	$(MAKE) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(KERNEL_DIR) M=$(PWD) clean
