ifneq ($(KERNELRELEASE),)
obj-m := modify_syscall.o       #obj-m指编译成外部模块
else
KERNELDIR := /lib/modules/$(shell uname -r)/build  #定义一个变量，指向内核目录
PWD := $(shell pwd)
 modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules  #编译内核模块
 endif
