#ifndef __PIPE_H__
#define __PIPE_H__

#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/types.h>

#define PROPER_FIFO_BEH_IDX	(3)

struct scull_pipe {
	size_t			 idx;
	wait_queue_head_t	 inq,  	 outq;
	wait_queue_head_t	 openq;
	char 			*buf, 	*end;
	size_t			 buf_len;
	char			*rp, 	*wp;
	size_t			 readers, writers;
	struct fasync_struct	*async_q;
	struct mutex	 	 lock;
	struct cdev		 cdev;
};

extern size_t	scull_p_len;
extern dev_t	scull_p_dev;

int scull_p_init(dev_t firstdev);
void scull_p_cleanup(void);

#endif
