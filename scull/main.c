#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>

#include "ioctl.h"
#include "pipe.h"
#include "proc.h"
#include "scull.h"

int 	scull_major = SCULL_MAJOR;
int 	scull_minor = 0;
ulong 	scull_nr_devs = SCULL_NR_DEVS;
ulong	scull_quantum = SCULL_QUANTUM;
ulong	scull_qset = SCULL_QSET;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, ulong, S_IRUGO);
module_param(scull_quantum, ulong, S_IRUGO);
module_param(scull_qset, ulong, S_IRUGO);

MODULE_AUTHOR("Alessandro Rubini, Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");

struct scull_dev *scull_devices;	/* allocated in scull_init_module */
struct file_operations scull_fops = {
	.owner =    THIS_MODULE,
	/*.llseek =   scull_llseek,*/
	.read =     scull_read,
	.write =    scull_write,
	.unlocked_ioctl = scull_ioctl,
	.open =     scull_open,
	/*.release =  scull_release,*/
};

/*
 * empty out scull device -> must be called with the device mutex held
 */
static void __scull_trim(struct scull_dev *dev) 
{
	struct 	scull_qset	*qset, *next;
	size_t	i;

	lockdep_assert_held(&dev->lock);

	for (qset = dev->qset; qset != NULL; qset = next) {
		if (qset->data != NULL) {
			for (i = 0; i < dev->qset_len; i++)
				kfree(qset->data[i]);
			kfree(qset->data);
			qset->data = NULL;
		}
		next = qset->next;
		kfree(qset);
	}
	dev->len = 0;
	dev->quantum_len = scull_quantum;
	dev->qset_len = scull_qset;
	dev->qset = NULL;
}

int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;

	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev;

	/* is it write only? then trim it */
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (mutex_lock_interruptible(&dev->lock))
			return (-ERESTARTSYS);
		__scull_trim(dev);
		mutex_unlock(&dev->lock);
	}
	return (0);
}

int scull_release(struct inode *inode, struct file *filp)
{

	return (0);
}

static struct scull_qset *__scull_follow(struct scull_dev *dev, 
		struct scull_follow *flw, const loff_t *f_pos)
{
	const 	size_t 	total_len = dev->quantum_len * dev->qset_len;
	const 	size_t	rest = *f_pos % total_len;
	struct	scull_qset *qset;
	size_t	n;

	lockdep_assert_held(&dev->lock);

	/* which qset */
	flw->qset_p = *f_pos / total_len;
	/* which quantum + offset */
	flw->quantum_p = rest / dev->quantum_len;
	flw->offset_p = rest % dev->quantum_len;

	qset = dev->qset;
	if (qset == NULL) {
		qset = dev->qset = kmalloc(sizeof(*dev->qset), GFP_KERNEL);
		if (qset == NULL)
			return (NULL);
		memset(qset, 0, sizeof(*dev->qset));
	}

	n = flw->qset_p;
	while(n--) {
		if (qset->next == NULL) {
			qset->next = kmalloc(sizeof(*qset->next), GFP_KERNEL);
			if (qset->next == NULL)
				return (NULL);
			memset(qset->next, 0, sizeof(*qset->next));
		}
		qset = qset->next;
	}
	return (qset);
}

static void __scull_meminfo(const struct scull_dev *dev)
{
	const 	struct scull_qset *qset = dev->qset;
	size_t 	i, mem = 0, tmem = 0;

	for (/* nothing */; qset != NULL; qset = qset->next) {
		tmem += sizeof(*qset);
		mem += sizeof(*qset);
		if (qset->data == NULL)
			continue;

		tmem += dev->qset_len * sizeof(*qset->data);
		for (i = 0; i < dev->qset_len; i++) {
			if (qset->data[i] == NULL)
				break;
			tmem += dev->quantum_len;
			mem += sizeof(*qset->data);
			mem += dev->len;
		}
	}
	pr_debug("mem usage for qset %p: total [%zu] kb use [%zu] bytes\n", 
	    dev->qset, tmem/1024, mem);
}

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, 
		loff_t *f_pos)
{
	struct	scull_dev 	*dev = filp->private_data;
	struct	scull_qset	*qset;
	struct	scull_follow	flw;
	ssize_t	ssret = 0;

	if (mutex_lock_interruptible(&dev->lock))
		return(-ERESTARTSYS);

	if (*f_pos >= dev->len)
		goto out;
	if (*f_pos + count > dev->len)
		count = dev->len - *f_pos;

	qset = __scull_follow(dev, &flw, f_pos);
	if (qset == NULL || qset->data == NULL ||
	    qset->data[flw.quantum_p] == NULL)
		goto out;

	/* read only up to the end of this quantum */
	if (count > (dev->quantum_len - flw.offset_p))
		count = dev->quantum_len - flw.offset_p;

	if (copy_to_user(buf, qset->data[flw.quantum_p] + flw.offset_p, count)) {
		ssret = -EFAULT;
		goto out;
	}
	*f_pos += count;
	ssret = count;
out:
	__scull_meminfo(dev);
	mutex_unlock(&dev->lock);
	return (ssret);
}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	struct	scull_dev 	*dev = filp->private_data;
	struct	scull_qset 	*qset;
	struct	scull_follow	flw;
	ssize_t ssret = -ENOMEM;

	if (mutex_lock_interruptible(&dev->lock))
		return (-ERESTARTSYS);

	qset = __scull_follow(dev, &flw, f_pos);
	if (qset == NULL)
		goto out;
	if (qset->data == NULL) {
		qset->data = kmalloc(dev->qset_len * sizeof(*qset->data), 
				GFP_KERNEL);
		if (qset->data == NULL)
			goto out;
		memset(qset->data, 0, dev->qset_len * sizeof(*qset->data));
	}
	if (qset->data[flw.quantum_p] == NULL) {
		qset->data[flw.quantum_p] = kmalloc(dev->quantum_len, 
				GFP_KERNEL);
		if (qset->data[flw.quantum_p] == NULL)
			goto out;
	}

	/* write only up to the end of this quantum */
	if (count > (dev->quantum_len - flw.offset_p))
		count = dev->quantum_len - flw.offset_p;

	if (copy_from_user(qset->data[flw.quantum_p] + flw.offset_p, buf,
				count)) {
		ssret = -EFAULT;
		goto out;
	}
	*f_pos += count;
	ssret = count;

	/* update size */
	if (dev->len < *f_pos)
		dev->len = *f_pos;

out:
	__scull_meminfo(dev);
	mutex_unlock(&dev->lock);
	return (ssret);
}

void scull_cleanup_module(void)
{
	const dev_t devno = MKDEV(scull_major, scull_minor);
	size_t i;

	if (scull_devices == NULL)
		goto final;

	for (i = 0; i < scull_nr_devs; i++) {
		struct scull_dev *dev = &scull_devices[i];

		mutex_lock(&dev->lock);	
		__scull_trim(dev);
		mutex_unlock(&dev->lock);
		cdev_del(&dev->cdev);
	}
	kfree(scull_devices);
final:
	scull_remove_proc();

	unregister_chrdev_region(devno, scull_nr_devs);
	scull_p_cleanup();
	/*scull_access_cleanup();*/
	pr_debug("offline\n");

}

static void scull_setup_cdev(struct scull_dev *dev, size_t i)
{
	const dev_t devno = MKDEV(scull_major, scull_minor + i);
	int err;

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		pr_notice("error %d adding scull%zu\n", err, i);
	else
		pr_notice("added device %u\n", devno);
}

int scull_init_module(void)
{
	dev_t dev;
	size_t i;
	int ret;

	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		ret = register_chrdev_region(dev, scull_nr_devs, "scull");
	} else {
		ret = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs,
				"scull");
		scull_major = MAJOR(dev);
	}
	if (ret < 0) {
		pr_warn("scull: can't get major %d\n", scull_major);
		return (ret);
	}

	scull_devices = kmalloc(scull_nr_devs * sizeof(*scull_devices), 
			GFP_KERNEL);
	if (scull_devices == NULL) {
		ret = -ENOMEM;
		goto fail;
	}
	memset(scull_devices, 0, scull_nr_devs * sizeof(*scull_devices));

	/* initialize each device */
	for (i = 0; i < scull_nr_devs; i++) {
		scull_devices[i].quantum_len = scull_quantum;
		scull_devices[i].qset_len = scull_qset;
		mutex_init(&scull_devices[i].lock);
		scull_setup_cdev(&scull_devices[i], i);
	}

	dev = MKDEV(scull_major, scull_minor + scull_nr_devs);
	dev += scull_p_init(dev); 
	/*dev += scull_access_init(dev);*/

	scull_create_proc();
	pr_debug("online major %d minor %d nr_devs %zu quantum %zu qset %zu\n",
			scull_major, scull_minor, scull_nr_devs, scull_quantum,
			scull_qset);
	return (0);
fail:
	scull_cleanup_module();
	return (ret);
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
