#ifndef __IOCTL_H__
#define __IOCTL_H__

long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#endif
