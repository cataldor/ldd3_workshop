# if KERNELRELEASE is defined, we've been invoked from the kernel build
# system and can use its language.
PROGNAME ?= qus

ifneq ($(KERNELRELEASE),)
	obj-m	:= qus.o


# otherwise we were called directly from the command line; invoke
# the kernel build system.
else

	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) C=1 W=1 modules

clean:
	rm $(PROGNAME).ko $(PROGNAME).mod $(PROGNAME).mod.c $(PROGNAME).mod.o $(PROGNAME).o Module.symvers modules.order
	rm .*.cmd

endif

