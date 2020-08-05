#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>

#include <asm/hardirq.h>

static uint32_t delay = HZ;

module_param(delay, uint, 0);

MODULE_AUTHOR("Alessandro Rubini");
MODULE_LICENSE("DUAL BSD/GPL");

/* use these as data pointers, to implement four files in one func */
enum jit_files {
	JIT_BUSY,
	JIT_SCHED,
	JIT_QUEUE,
	JIT_SCHEDTO
};

/*
 * this function prints one line of data, after sleeping one second.
 * it can sleep in different ways, according to the data ptr
 */
static int jit_fn_show(struct seq_file *m, void *v)
{
	const enum jit_files data_ptr = (const enum jit_files)m->private;
	unsigned long		j0,	j1;
	wait_queue_head_t	wait;

	j0 = jiffies;
	j1 = j0 + delay;

	switch (data_ptr) {
	case JIT_BUSY:
		while (time_before(jiffies, j1))
			cpu_relax();
		break;
	case JIT_SCHED:
		while (time_before(jiffies, j1))
			schedule();
		break;
	case JIT_QUEUE:
		init_waitqueue_head(&wait);
		(void)wait_event_interruptible_timeout(wait, 0, delay);
		break;
	case JIT_SCHEDTO:
		set_current_state(TASK_INTERRUPTIBLE);
		(void)schedule_timeout(delay);
		break;
	default:
		pr_notice("unknown data_ptr %u\n", data_ptr);
		break;
	}
	/* real delayed value */
	j1 = jiffies;
	/* diff may overflow */
	seq_printf(m, "%9li %9li [diff %9li]\n", j0, j1, j1 - (j0 + delay));
	return (0);
}

static int jit_fn_open(struct inode *inode, struct file *file)
{
	return single_open(file, jit_fn_show, PDE_DATA(inode));
}

static const struct proc_ops jit_fn_fops = {
	.proc_open 	=	jit_fn_open,
	.proc_read	=	seq_read,
	.proc_lseek =	seq_lseek,
	.proc_release =	single_release,
};

/*
 * this file, on the other hand, returns the current time forever
 */
static int jit_currenttime_show(struct seq_file *m, void *v)
{
	struct	timespec64 tv1;
	struct	timespec64 tv2;
	unsigned long	j1;
	u64	j2;

	/* four ways to get the same thing */
	j1 = jiffies;
	j2 = get_jiffies_64();
	ktime_get_real_ts64(&tv1);
	ktime_get_coarse_real_ts64(&tv2);	

	seq_printf(m, "0x%08lx 0x%016llx %10llu.%06lu %40llu.%09lu\n",
			j1, j2, tv1.tv_sec, tv1.tv_nsec, tv2.tv_sec,
			tv2.tv_nsec);
	return (0);
}

static int jit_currenttime_open(struct inode *inode, struct file *file)
{
	return single_open(file, jit_currenttime_show, NULL);
}

static const struct proc_ops jit_currenttime_fops = {
	.proc_open	  =	jit_currenttime_open,
	.proc_read	  =	seq_read,
	.proc_lseek   	  =	seq_lseek,
	.proc_release     =	single_release,
};

/*
 * the timer example follows
 */

static uint32_t tdelay = 10;
module_param(tdelay, uint, 0);

struct jit_data {
	struct timer_list	 timer;
	struct tasklet_struct	 tlet;
	struct seq_file		*m;
	int 			 hi; /* tasklet or tasklet_hi */
	wait_queue_head_t	 wait;
	unsigned long		 prev_jiffies;
	size_t			 loops;
};
#define JIT_ASYNC_LOOPS 5

static void jit_timer_fn(struct timer_list *t)
{
	struct jit_data *data = from_timer(data, t, timer); 
	const unsigned long	 j = jiffies;

	seq_printf(data->m, "%9li  %3li     %lu    %lu    %6i    %i   %s\n",
			j, j - data->prev_jiffies, 
			softirq_count(), hardirq_count(),
			current->pid, smp_processor_id(), current->comm);

	if(--data->loops) {
		data->timer.expires += tdelay;
		data->prev_jiffies = j;
		add_timer(&data->timer);
	} else
		wake_up_interruptible(&data->wait);
}

static int jit_timer_show(struct seq_file *m, void *v)
{
	struct jit_data *data;
	const unsigned long j = jiffies;

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return (-ENOMEM);

	init_waitqueue_head(&data->wait);

	/* write the first lines in the buffer */
	seq_puts(m, "   time   delta  insrq    inhrq    pid   cpu command\n");
	seq_printf(m, "%9li  %3li     %lu    %lu    %6i   %i   %s\n",
			j, 0L, softirq_count(), hardirq_count(),
			current->pid, smp_processor_id(), current->comm);

	/* data for our functions */
	data->prev_jiffies = j;
	data->m = m;
	data->loops = JIT_ASYNC_LOOPS;

	/* register the timer */
	timer_setup(&data->timer, jit_timer_fn, 0);
	data->timer.expires = j + tdelay;
	add_timer(&data->timer);

	/* wait for buffer to fill */
	wait_event_interruptible(data->wait, !data->loops);
	if (signal_pending(current))
		return (-ERESTARTSYS);
	kfree(data);
	return (0);
}

static int jit_timer_open(struct inode *inode, struct file *file)
{
	return single_open(file, jit_timer_show, NULL);
}

static const struct proc_ops jit_timer_fops = {
	.proc_open		=	jit_timer_open,
	.proc_read		=	seq_read,
	.proc_lseek		=	seq_lseek,
	.proc_release		=	single_release,
};

static void jit_tasklet_fn(unsigned long arg)
{
	struct jit_data *data = (struct jit_data *)arg;
	const unsigned long j = jiffies;

	seq_printf(data->m, "%9li  %3li     %li    %6i   %i   %s\n",
			j, j - data->prev_jiffies, in_interrupt(), current->pid,
			smp_processor_id(), current->comm);

	if (--data->loops) {
		data->prev_jiffies = j;
		if (data->hi)
			tasklet_hi_schedule(&data->tlet);
		else
			tasklet_schedule(&data->tlet);
	} else 
		wake_up_interruptible(&data->wait);
}

static int jit_tasklet_show(struct seq_file *m, void *v)
{
	struct jit_data *data;
	const unsigned long j = jiffies;
	const long hi = (long)m->private;

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return (-ENOMEM);

	init_waitqueue_head(&data->wait);

	/* write the first lines in the buffer */
	seq_printf(m, "   time   delta  inirq    pid   cpu command\n");
	seq_printf(m, "%9li  %3li     %i    %6i   %i   %s\n",
			j, 0L, in_interrupt() ? 1 : 0,
			current->pid, smp_processor_id(), current->comm);

	data->prev_jiffies = j;
	data->m = m;
	data->loops = JIT_ASYNC_LOOPS;

	/* register tasklet */
	tasklet_init(&data->tlet, jit_tasklet_fn, (unsigned long)data);
	data->hi = hi;
	if (hi)
		tasklet_hi_schedule(&data->tlet);
	else
		tasklet_schedule(&data->tlet);

	/* wait for buffer to fill */
	wait_event_interruptible(data->wait, !data->loops);

	if (signal_pending(current))
		return (-ERESTARTSYS);

	kfree(data);
	return (0);
}

static int jit_tasklet_open(struct inode *inode, struct file *file)
{
	return single_open(file, jit_tasklet_show, PDE_DATA(inode));
}

static const struct proc_ops jit_tasklet_fops = {
	.proc_open 	=	jit_tasklet_open,
	.proc_read	=	seq_read,
	.proc_lseek	=	seq_lseek,
	.proc_release	=	single_release,
};


static int __init jit_init(void)
{
	proc_create_data("currentime", 0, NULL, &jit_currenttime_fops, NULL);
	proc_create_data("jitbusy", 0, NULL, &jit_fn_fops, (void *)JIT_BUSY);
	proc_create_data("jitsched", 0, NULL, &jit_fn_fops, (void *)JIT_SCHED);
	proc_create_data("jitqueue", 0, NULL, &jit_fn_fops, (void *)JIT_QUEUE);
	proc_create_data("jitschedto", 0, NULL, &jit_fn_fops,
		       	(void *)JIT_SCHEDTO);

	proc_create_data("jitimer", 0, NULL, &jit_timer_fops, NULL);
	proc_create_data("jitasklet", 0, NULL, &jit_tasklet_fops, NULL);
	proc_create_data("jitasklethi", 0, NULL, &jit_tasklet_fops, (void *)1);

	return 0;
}

static void __exit jit_cleanup(void)
{
	remove_proc_entry("currentime", NULL);
	remove_proc_entry("jitbusy", NULL);
	remove_proc_entry("jitsched", NULL);
	remove_proc_entry("jitqueue", NULL);
	remove_proc_entry("jitschedto", NULL);

	remove_proc_entry("jitimer", NULL);
	remove_proc_entry("jitasklet", NULL);
	remove_proc_entry("jitasklethi", NULL);
}

module_init(jit_init);
module_exit(jit_cleanup);
