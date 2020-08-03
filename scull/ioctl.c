#include <linux/capability.h>
#include <linux/uaccess.h>

#include "ioctl.h"
#include "pipe.h"
#include "scull.h"

long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int tmp;
	int ret;

	ret = 0;

	/*
	 * extract the type and number bitfields
	 * don't decode wrong cmds
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC)
		return (-ENOTTY);
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR)
		return (-ENOTTY);

	/**
	 * calling access_ok directly, allows the use of __put_user,
	 * __get_user
	 **/
	ret = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (ret)
		return (-EFAULT);

	switch (cmd) {
	case SCULL_IOCRESET:
		scull_quantum = SCULL_QUANTUM;
		scull_qset = SCULL_QSET;
		break;
	case SCULL_IOCSQUANTUM:
		if (!capable(CAP_SYS_ADMIN))
			return (-EPERM);
		ret = __get_user(scull_quantum, (int __user *)arg);
		break;
	case SCULL_IOCTQUANTUM:
		if (!capable(CAP_SYS_ADMIN))
			return(-EPERM);
		scull_quantum = arg;
		break;
	case SCULL_IOCGQUANTUM:
		ret = __put_user(scull_quantum, (int __user *)arg);
		break;
	case SCULL_IOCQQUANTUM:
		return (scull_quantum);
		/* NOTREACHED */
		break;
	case SCULL_IOCXQUANTUM:
		if (!capable(CAP_SYS_ADMIN))
			return(-EPERM);
		tmp = scull_quantum;
		ret = __get_user(scull_quantum, (int __user *)arg);
		if (ret == 0)
			ret = __put_user(tmp, (int __user *)arg);
		break;
	case SCULL_IOCHQUANTUM:
		if (!capable(CAP_SYS_ADMIN))
			return(-EPERM);
		tmp = scull_quantum;
		scull_quantum = arg;
		return (tmp);
		/* NOTREACHED */
		break;
        /*
         * The following two change the buffer size for scullpipe.
         * The scullpipe device uses this same ioctl method, just to
         * write less code. Actually, it's the same driver, isn't it?
         */

	  case SCULL_P_IOCTSIZE:
		scull_p_len = arg;
		break;
	  case SCULL_P_IOCQSIZE:
		return (scull_p_len);
		break;
	default:
		return (-ENOTTY);
	}
	return (ret);
}

