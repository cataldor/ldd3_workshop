#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>

#include "ioctl.h"
#include "mutex_sparse.h"
#include "scull.h"		/* local definitions */
#include "pipe.h"

static size_t scull_p_nr_devs = SCULL_P_NR_DEVS;
size_t scull_p_len = SCULL_P_LEN;
dev_t scull_p_dev;

module_param(scull_p_nr_devs, ulong, 0);
module_param(scull_p_len, ulong, 0);

static struct scull_pipe *scull_p_devices;

static int scull_p_fasync(int fd, struct file *filp, int mode);
static size_t spacefree(struct scull_pipe *dev);


static int scull_p_proper_open(struct scull_pipe *dev, struct inode *inode,
	       	struct file *filp)
{

	if (__mutex_lock_interruptible_sparse(&dev->lock))
		return (-ERESTARTSYS);

	if (filp->f_mode & FMODE_READ) {
		if (filp->f_flags & O_NONBLOCK && dev->writers == 0) {
			__mutex_unlock_sparse(&dev->lock);
			return (-EAGAIN);
		}

		dev->readers++;
		while (dev->writers == 0) {
			__mutex_unlock_sparse(&dev->lock);
			pr_notice("%s waiting for writers\n", current->comm);
			if (wait_event_interruptible(dev->openq, 
			    dev->writers > 0))
				return (-ERESTARTSYS);
			if (__mutex_lock_interruptible_sparse(&dev->lock))
				return (-ERESTARTSYS);
		}
	} else {
		dev->writers++;
		if (dev->buf == NULL) {
			dev->buf = kmalloc(scull_p_len, GFP_KERNEL);
			if (dev->buf == NULL) {
				__mutex_unlock_sparse(&dev->lock);
				return (-ENOMEM);
			}
		}
		dev->buf_len = scull_p_len;
		dev->end = dev->buf + dev->buf_len;
		dev->rp = dev->wp = dev->buf;

		if (dev->readers != 0)
			wake_up_interruptible_sync(&dev->openq);
	}
	__mutex_unlock_sparse(&dev->lock);
	return (nonseekable_open(inode, filp));

}

static int scull_p_open(struct inode *inode, struct file *filp)
{
	struct scull_pipe *dev;

	dev = container_of(inode->i_cdev, struct scull_pipe, cdev);
	filp->private_data = dev;

	if (dev->idx == PROPER_FIFO_BEH_IDX) {
		pr_notice("proper fifo behavior for scullpipe %zu\n", dev->idx);
		return (scull_p_proper_open(dev, inode, filp));
	}

	if (__mutex_lock_interruptible_sparse(&dev->lock))
		return(-ERESTARTSYS);

	if (dev->buf == NULL) {
		dev->buf = kmalloc(scull_p_len, GFP_KERNEL);
		if (dev->buf == NULL) {
			__mutex_unlock_sparse(&dev->lock);
			return (-ENOMEM);
		}
	}
	dev->buf_len = scull_p_len;
	dev->end = dev->buf + dev->buf_len;
	dev->rp = dev->wp = dev->buf;

	/* use f_mode, not f_flags: it's cleaner (fs/open.c tells why) */
	if (filp->f_mode & FMODE_READ)
		dev->readers++;
	if (filp->f_mode & FMODE_WRITE)
		dev->writers++;

	__mutex_unlock_sparse(&dev->lock);
	return (nonseekable_open(inode, filp));
}

static int scull_p_release(struct inode *inode, struct file *filp)
{
	struct scull_pipe *dev = filp->private_data;	

	/* remove this filp from the async notified filps */
	(void)scull_p_fasync(-1, filp, 0);
	if (__mutex_lock_interruptible_sparse(&dev->lock))
		return (-ERESTARTSYS);

	if (filp->f_mode & FMODE_READ)
		dev->readers--;
	if (filp->f_mode & FMODE_WRITE)
		dev->writers--;
	if (dev->readers + dev->writers == 0) {
		kfree(dev->buf);
		dev->buf = NULL;
	}
	__mutex_unlock_sparse(&dev->lock);
	return (0);
}

static size_t spacefree(struct scull_pipe *dev)
	__must_hold(&dev->lock)
{

	lockdep_assert_held(&dev->lock);
	if (dev->rp == dev->wp)
		return (dev->buf_len - 1);
	return (((dev->rp + dev->buf_len - dev->wp) % dev->buf_len) - 1);
}

static int scull_p_fasync(int fd, struct file *filp, int mode)
{
	struct scull_pipe *dev = filp->private_data;

	return (fasync_helper(fd, filp, mode, &dev->async_q));
}

static ssize_t scull_p_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_ops)
{
	struct scull_pipe *dev = filp->private_data;

	if (__mutex_lock_interruptible_sparse(&dev->lock))
		return (-ERESTARTSYS);

	pr_debug(KERN_NOTICE "%s %lu %lu\n", current->comm, dev->rp - dev->buf,
	    dev->wp - dev->buf);

	if (dev->idx == PROPER_FIFO_BEH_IDX) {
		if (dev->rp == dev->wp && dev->writers == 0) {
			__mutex_unlock_sparse(&dev->lock);
			return (0);
		}
	}
	while (dev->rp == dev->wp) {
		__mutex_unlock_sparse(&dev->lock);
		if (filp->f_flags & O_NONBLOCK)
			return (-EAGAIN);
		pr_notice("%s going to sleep r%zd w%zd\n", 
				current->comm, dev->readers, dev->writers);
		if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
			return (-ERESTARTSYS);
		if (__mutex_lock_interruptible_sparse(&dev->lock))
			return (-ERESTARTSYS);
	}
	/* ok data available */
	if (dev->wp > dev->rp)
		count = min(count, (size_t)(dev->wp - dev->rp));
	/* write pointer has wrapped */
	else
		count = min(count, (size_t)(dev->end - dev->rp));

	if (copy_to_user(buf, dev->rp, count)) {
		__mutex_unlock_sparse(&dev->lock);
		return (-EFAULT);
	}
	dev->rp += count;
	if (dev->rp == dev->end)
		dev->rp = dev->buf;
	__mutex_unlock_sparse(&dev->lock);
	/* awake any writers */
	wake_up_interruptible(&dev->outq);
	pr_notice("%s did read %zu bytes\n", current->comm, count);
	pr_notice("%s %lu %lu\n", current->comm, dev->rp - dev->buf,
	    dev->wp - dev->buf);
	return (count);
}

static int scull_getwritespace(struct scull_pipe *dev, struct file *filp)
{

	pr_notice("%s %lu %lu\n", current->comm, dev->rp - dev->buf,
	    dev->wp - dev->buf);
	lockdep_assert_held(&dev->lock);
	__acquire(&dev->lock);

	while (spacefree(dev) == 0) {
		DEFINE_WAIT(wait);

		__mutex_unlock_sparse(&dev->lock);
		if (filp->f_flags & O_NONBLOCK)
			return (-EAGAIN);

		pr_debug("%s going to sleep\n", current->comm);
		pr_debug("%s r %zd w %zd\n", current->comm, dev->readers,
		    dev->writers);
		prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
		if (spacefree(dev) == 0)
			schedule();
		finish_wait(&dev->outq, &wait);
		if (signal_pending(current))
			return (-ERESTARTSYS);
		if (__mutex_lock_interruptible_sparse(&dev->lock))
			return (-ERESTARTSYS);
	}
	__release(&dev->lock);
	return (0);
}

static ssize_t scull_p_write(struct file *filp, const char __user *buf, 
		size_t count, loff_t *f_pos)
{
	struct scull_pipe	*dev = filp->private_data;
	int ret;

	if (__mutex_lock_interruptible_sparse(&dev->lock))
		return (-ERESTARTSYS);

	/* releases lock if it fails */
	ret = scull_getwritespace(dev, filp);
	if (ret) {
		/* make sparse happy */
		__release(&dev->lock);
		return (ret);
	}

	count = min(count, (size_t)spacefree(dev));
	if (dev->wp >= dev->rp)
		count = min(count, (size_t)(dev->end - dev->wp));
	else
		count = min(count, (size_t)(dev->rp - dev->wp - 1));
	if  (copy_from_user(dev->wp, buf, count)) {
		__mutex_unlock_sparse(&dev->lock);
		return (-EFAULT);
	}
	dev->wp += count;
	if (dev->wp == dev->end)
		dev->wp = dev->buf;

	__mutex_unlock_sparse(&dev->lock);
	wake_up_interruptible(&dev->inq);

	if (dev->async_q != NULL)
		kill_fasync(&dev->async_q, SIGIO, POLL_IN);
	pr_debug("%s wrote %zu bytes\n", current->comm, count);
	return (count);
}

static __poll_t scull_p_poll(struct file *filp, poll_table *wait)
{
	struct scull_pipe *dev = filp->private_data;
	__poll_t mask = 0;

	__mutex_lock_sparse(&dev->lock);
	poll_wait(filp, &dev->inq, wait);
	poll_wait(filp, &dev->outq, wait);

	if (dev->idx == PROPER_FIFO_BEH_IDX && dev->writers == 0) {
		__mutex_unlock_sparse(&dev->lock);
		return ((__force __poll_t)(POLLERR | POLLHUP));
	}

	/*
	 * The buffer is circular; it is considered full
	 * if "wp" is right behind "rp" and empty if the
	 * two are equal.
	 */
	if (dev->rp != dev->wp)
		mask |= (__force __poll_t)(POLLIN | POLLRDNORM);
	if (spacefree(dev))
		mask |= (__force __poll_t)(POLLOUT | POLLWRNORM);
	__mutex_unlock_sparse(&dev->lock);
	return (mask);
}

static struct file_operations scull_pipe_fops = {
	.owner = 	THIS_MODULE,
	.llseek = 	no_llseek,
	.read = 	scull_p_read,
	.write =	scull_p_write,
	.poll = 	scull_p_poll,
	.unlocked_ioctl = scull_ioctl,
	.open = 	scull_p_open,
	.release =	scull_p_release,
	.fasync =	scull_p_fasync,
};

static void scull_p_setup_cdev(struct scull_pipe *dev, size_t idx)
{
	const dev_t devno = scull_p_dev + idx;
	int err;

	cdev_init(&dev->cdev, &scull_pipe_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		pr_notice("error %d adding scullpipe%zu\n", err, idx);
}

int scull_p_init(dev_t firstdev)
{
	size_t i;
	int ret;

	ret = register_chrdev_region(firstdev, scull_p_nr_devs, "scullp");
	if (ret < 0) {
		pr_notice("unable to get scullp region, %d\n", ret);
		return (0);
	}

	scull_p_dev = firstdev;
	scull_p_devices = kmalloc(scull_p_nr_devs * sizeof(*scull_p_devices),
			GFP_KERNEL);
	if (scull_p_devices == NULL) {
		unregister_chrdev_region(firstdev, scull_p_nr_devs);
		return (0);
	}
	memset(scull_p_devices, 0, scull_p_nr_devs * sizeof(*scull_p_devices));
	for (i = 0; i < scull_p_nr_devs; i++) {
		struct scull_pipe *p = &scull_p_devices[i];

		p->idx = i;
		init_waitqueue_head(&p->inq);
		init_waitqueue_head(&p->outq);
		init_waitqueue_head(&p->openq);
		mutex_init(&p->lock);
		scull_p_setup_cdev(p, i);
		pr_debug("added scullp %zu\n", firstdev + i);
	}
	/* XXX: seq file */
	return (scull_p_nr_devs);
}

void scull_p_cleanup(void)
{
	size_t i;

	/* XXX seqfile */

	if (scull_p_devices == NULL)
		return;

	for (i = 0; i < scull_p_nr_devs; i++) {
		cdev_del(&scull_p_devices[i].cdev);
		kfree(scull_p_devices[i].buf);
	}
	kfree(scull_p_devices);
	unregister_chrdev_region(scull_p_dev, scull_p_nr_devs);
	scull_p_devices = NULL;
}

