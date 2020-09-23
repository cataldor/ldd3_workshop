# ldd3 workshop

This repository is a collection of modified LDD3's device drivers. It is based
on the original and martinezjavier's [1] updated source code.

The objective of this repository is to try out some of the kernel features not
explored in the aforementioned versions.

These drivers were developed for Linux Kernel v5.8 and GCC 10.

The next headings details the changes made to the device drivers:

## scull

This driver contains the scull + scullp + scullc driver functionality.

The following changes were made:
* Multiple C files (ioctl, pipe, proc);
* Quantum size changed to unsigned;
* Use of lockdep for lock-related assumptions;
* Procedure `scull_follow` receives a struct now;
* Use of dynamic debugging (`dyndbg`) for printing;
* Procedure `scull_meminfo`;
* Implemented 'proper' FIFO behavior for pipe nr `PROPER_FIFO_BEH_IDX`;
* Support for the semantic parser sparse.


## jit

* Minor changes (const, testing other irq macros).

## jiq

* const, `from_tasklet`;
* Avoid race condition with jitimer by using a flag.

## edu (WIP)

driver for QEMU edu educational device [2].

- [x] Initialization and termination
- [x] Inverse function
- [x] IRQ and MSI setup
- [x] Timeout support for IRQ/MSI
- [x] Test IRQ/MSI
- [ ] DMA setup
- [ ] Test DMA
- [ ] Factorial function (irq/irqless)
- [ ] devm (i.e., managed resource API)

## qus (QEMU usb mass storage driver) (WIP)

driver for the QEMU usb mass storage driver [3]. This driver is for research
only (Linux already supports the QEMU device via usb-storage.c).

- [x] Initialization and termination
- [x] Control endpoint
- [ ] fops

[1]: https://github.com/martinezjavier/ldd3
[2]: https://github.com/qemu/qemu/blob/master/docs/specs/edu.txt
[3]: https://github.com/qemu/qemu/blob/master/hw/usb/dev-storage.c
