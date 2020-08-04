#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "mutex_sparse.h"
#include "scull.h"

static void *scull_seq_start(struct seq_file *s, loff_t *pos)
{

	if (*pos >= scull_nr_devs)
		return (NULL);
	return (&scull_devices[*pos]);
}

static void *scull_seq_next(struct seq_file *s, void *v, loff_t *pos)
{

	(*pos)++;
	if (*pos >= scull_nr_devs)
		return(NULL);
	return(&scull_devices[*pos]);
}

static void __scull_print_ascii(struct seq_file *s, void *data, size_t data_len)
{
	const char *buf = (const char *)data;
	size_t i;

	for (i = 0; i < (data_len / 30); i++) {
		seq_printf(s, "%c", isalpha(*buf) ? *buf : '.');
		buf++;
		if ((i + 1) % 64 == 0)
			seq_printf(s, "\n");
	}
	seq_printf(s, "\n");


}
static int scull_seq_show(struct seq_file *s, void *v)
{
	struct scull_dev	*dev = (struct scull_dev *)v;
	const 	struct scull_qset 	*qset;
	size_t 	i;

	if (__mutex_lock_interruptible_sparse(&dev->lock))
		return(-ERESTARTSYS);
	seq_printf(s, "\ndevice %zu: qset %zu, q %zu, sz %zu\n",
			(size_t) (dev - scull_devices), dev->qset_len,
			dev->quantum_len, dev->len);
	for (qset = dev->qset; qset != NULL; qset = qset->next) {
		seq_printf(s, "  item at %p, qset at %p\n", qset, qset->data);
		if (qset->data != NULL && qset->next == NULL)
			for (i = 0; i < dev->qset_len; i++) {
				if (qset->data[i] != NULL) {
					seq_printf(s, "    %4zd: %8p\n",
							i, qset->data[i]);
					__scull_print_ascii(s, qset->data[i],
							dev->quantum_len * 
							sizeof(*qset->data));

				}
			}
	}
	__mutex_unlock_sparse(&dev->lock);
	return (0);
}

static void scull_seq_stop(struct seq_file *s, void *v)
{

	return;
}

static struct seq_operations scull_seq_ops = {
	.start = scull_seq_start,
	.next  = scull_seq_next,
	.stop  = scull_seq_stop,
	.show  = scull_seq_show
};


static int scullseq_proc_open(struct inode *inode, struct file *filp)
{

	return (seq_open(filp, &scull_seq_ops));
}

static struct proc_ops scullseq_proc_ops = {
	.proc_open = scullseq_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

void scull_create_proc(void)
{
	proc_create("scullmem", 0, NULL, &scullseq_proc_ops);
}

void scull_remove_proc(void)
{
	remove_proc_entry("scullmem", NULL);
	return;
}

