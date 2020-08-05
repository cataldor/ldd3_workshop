# if KERNELRELEASE is defined, we've been invoked from the kernel build
# system and can use its language.
PROGNAME ?= jiq

ifneq ($(KERNELRELEASE),)
	scull-objs := jiq.o
	obj-m	:= jiq.o


# otherwise we were called directly from the command line; invoke
# the kernel build system.
else

	KERNELDIR ?= /home/cataldo/down/linux/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) C=1 W=1 modules

clean:
	rm $(PROGNAME).ko $(PROGNAME).mod.c $(PROGNAME).mod.o $(PROGNAME).o Module.symvers
	rm $(PROGNAME).mod modules.order .*.cmd
endif

