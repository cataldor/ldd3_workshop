#ifndef __MUTEX_SPARSE_H__
#define __MUTEX_SPARSE_H__

#include <linux/mutex.h>
/*
 * mutex_lock does not use sparse; thus we provide our own
 */
static inline int __mutex_lock_interruptible_sparse(struct mutex *lock)
	__acquires(lock)
{
	int ret = mutex_lock_interruptible(lock);
	if (!ret)
		__acquire(lock);

	return (ret);
}

static inline void __mutex_lock_sparse(struct mutex *lock)
	__acquires(lock)
{
	__acquire(lock);
	mutex_lock(lock);
}

static inline void __mutex_unlock_sparse(struct mutex *lock)
	__releases(lock)
{

	__release(lock);
	mutex_unlock(lock);
}

#endif
