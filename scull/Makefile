# if KERNELRELEASE is defined, we've been invoked from the kernel build
# system and can use its language.
PROGNAME ?= scull

ifneq ($(KERNELRELEASE),)
	scull-objs := main.o proc.o ioctl.o pipe.o
	obj-m	:= scull.o


# otherwise we were called directly from the command line; invoke
# the kernel build system.
else

	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) W=1 modules

clean:
	rm $(PROGNAME).ko $(PROGNAME).mod $(PROGNAME).mod.c $(PROGNAME).mod.o $(PROGNAME).o Module.symvers modules.order
	rm main.o proc.o ioctl.o pipe.o
	rm .*.cmd

endif

