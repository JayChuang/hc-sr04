obj-m := hc-sr04.o

ifeq ($(KERNELDIR),)
	KERNELDIR=/home/jay/workspace/linux
#	KERNELDIR=/lib/modules/$(shell uname -r)/build
endif

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
