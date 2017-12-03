obj-m += sciteex_gpio.o
KDIR = /lib/modules/$(shell uname -r)/build
all:
	make -C $(KDIR) M=$(PWD) modules

install:
	make INSTALL_MOD_DIR=sciteex -C $(KDIR) M=$(PWD) modules_install
	
clean:
	make -C $(KDIR) M=$(PWD) clean
# 
