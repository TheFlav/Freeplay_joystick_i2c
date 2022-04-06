KERNEL_DIR := /lib/modules/$(shell uname -r)/build

obj-m := freeplay-joystick.o

driver:
	make -C $(KERNEL_DIR) M=`pwd` modules

install: driver
	cp $(shell pwd)/freeplay-joystick.ko /lib/modules/$(shell uname -r)/kernel/drivers/input/joystick/
	depmod -a
	@echo "Be sure to add i2c-dev and freeplay-joystick to /etc/modules"
clean:
	make -C $(KERNEL_DIR) M=`pwd` clean
