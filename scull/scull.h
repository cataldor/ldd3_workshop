#ifndef __SCULL_H__
#define __SCULL_H__

/* IOW, IOR, etc */
#include <linux/cdev.h>
#include <linux/ioctl.h>

/* dynamic major by default */
#ifndef SCULL_MAJOR
#define SCULL_MAJOR		0
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS		4
#endif

/* pipe */
#ifndef SCULL_P_NR_DEVS
#define SCULL_P_NR_DEVS		4
#endif

/*
 * The bare device is a variable-length region of memory.
 * Use a linked list of indirect blocks.
 *
 * "scull_dev->qset" points to an array of pointers, each
 * pointer refers to a memory area of SCULL_QUANTUM bytes.
 *
 * The array (quantum-set) is SCULL_QSET long.
 */
#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM		4000
#endif

#ifndef SCULL_QSET
#define SCULL_QSET		1000
#endif

/* circular buffer */
#ifndef SCULL_P_LEN
#define SCULL_P_LEN		4000
#endif

/*
 * representation of scull quantum sets
 */
struct scull_qset {
	void 	**data;
	struct	scull_qset	*next;
};

struct scull_dev {
	struct	scull_qset	*qset; 	/* point to first quantum set */
	size_t	quantum_len;		/* the current quantum size */
	size_t	qset_len;		/* the current array size */
	size_t	len;			/* amount of data stored here */
	u32	access_key;		/* used by sculluid and scullpriv */
	struct	mutex	lock;
	struct	cdev		cdev;
};

struct scull_follow {
	size_t	qset_p;
	size_t	quantum_p;
	size_t  offset_p;
};

/*
 * split minors in two parts
 */
#define TYPE(minor)	(((minor) >> 4) & 0xF)
#define NUM(minor)	((minor) & 0xF)

extern int 	scull_major;
extern size_t 	scull_nr_devs;
extern size_t 	scull_quantum;
extern size_t	scull_qset;
extern struct scull_dev *scull_devices;


/* ioctl */
/* use 'k' as magic number */
#define SCULL_IOC_MAGIC 	'k'
#define SCULL_IOCRESET		_IO(SCULL_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SCULL_IOCSQUANTUM 	_IOW(SCULL_IOC_MAGIC,  1, int)
#define SCULL_IOCSQSET    	_IOW(SCULL_IOC_MAGIC,  2, int)
#define SCULL_IOCTQUANTUM 	_IO(SCULL_IOC_MAGIC,   3)
#define SCULL_IOCTQSET    	_IO(SCULL_IOC_MAGIC,   4)
#define SCULL_IOCGQUANTUM 	_IOR(SCULL_IOC_MAGIC,  5, int)
#define SCULL_IOCGQSET    	_IOR(SCULL_IOC_MAGIC,  6, int)
#define SCULL_IOCQQUANTUM 	_IO(SCULL_IOC_MAGIC,   7)
#define SCULL_IOCQQSET    	_IO(SCULL_IOC_MAGIC,   8)
#define SCULL_IOCXQUANTUM 	_IOWR(SCULL_IOC_MAGIC, 9, int)
#define SCULL_IOCXQSET    	_IOWR(SCULL_IOC_MAGIC,10, int)
#define SCULL_IOCHQUANTUM 	_IO(SCULL_IOC_MAGIC,  11)
#define SCULL_IOCHQSET    	_IO(SCULL_IOC_MAGIC,  12)

/*
 * The other entities only have "Tell" and "Query", because they're
 * not printed in the book, and there's no need to have all six.
 * (The previous stuff was only there to show different ways to do it.
 */
#define SCULL_P_IOCTSIZE 	_IO(SCULL_IOC_MAGIC,   13)
#define SCULL_P_IOCQSIZE 	_IO(SCULL_IOC_MAGIC,   14)
/* ... more to come */

#define SCULL_IOC_MAXNR 	14
void scull_cleanup_module(void);
int scull_init_module(void);
int scull_open(struct inode *inode, struct file *filp);
int scull_release(struct inode *inode, struct file *filp);
ssize_t scull_read(struct file *filp, char __user *buf, size_t count, 
		loff_t *f_pos);
ssize_t scull_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos);
void scull_create_proc(void);
void scull_remove_proc(void);

#endif
