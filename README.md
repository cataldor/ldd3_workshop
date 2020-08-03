# ldd3 workshop

This repository is a collection of modified LDD3's device drivers. It is based
on the original and martinezjavier's [1] updated source code.

The objective of this repository is to try out some of the kernel features not
explored in the aforementioned versions.

These drivers were developed for Linux Kernel v5.7 and GCC 10.

The next headings details the changes made to the device drivers:

## scull

This driver contains the scull + scullp driver functionality.

The following changes were made:
* Multiple C files (ioctl, pipe, proc);
* Quantum size changed to unsigned;
* Use of lockdep for lock-related assumptions;
* Procedure `scull_follow` receives a struct now;
* Use of dynamic debugging (`dyndbg`) for printing;
* Procedure `scull_meminfo`;
* Implemented 'proper' FIFO behavior for pipe nr `PROPER_FIFO_BEH_IDX`;

[1]: https://github.com/martinezjavier/ldd3
