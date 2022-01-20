KERNEL_DIR := /lib/modules/$(shell uname -r)/build

obj-m := freeplay-joystick.o

driver:
	make -C $(KERNEL_DIR) M=`pwd` modules

clean:
	make -C $(KERNEL_DIR) M=`pwd` clean
