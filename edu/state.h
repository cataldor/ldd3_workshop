#ifndef __STATE_H_
#define __STATE_H_

#include <linux/bits.h>
#include <linux/build_bug.h>

#include "hw.h"

static inline bool __qedu_is_irq_done(const struct qedu_device *edu)
{
	return test_bit(QEDU_STATE_IRQ_OK, &edu->state) ||
	    test_bit(QEDU_STATE_IRQ_TIMEDOUT, &edu->state) ||
	    test_bit(QEDU_STATE_SHUTDOWN, &edu->state);
}

static inline bool qedu_is_irq_done(const struct qedu_device *edu)
	__must_hold(&edu->lock)
{
	lockdep_assert_held(&edu->lock);

	return __qedu_is_irq_done(edu);
}

static inline bool qedu_is_shutting_down(const struct qedu_device *edu)
	__must_hold(&edu->lock)
{
	lockdep_assert_held(&edu->lock);
	return test_bit(QEDU_STATE_SHUTDOWN, &edu->state);
}

static inline bool qedu_is_waiting_irq(const struct qedu_device *edu)
{
	return test_bit(QEDU_STATE_IRQ_WAIT, &edu->state);
}

static inline void qedu_set_waiting_irq(struct qedu_device *edu)
{
	set_bit(QEDU_STATE_IRQ_WAIT, &edu->state);
	edu->timer.expires = jiffies + QEDU_TIMER_TIMEOUT;
	add_timer(&edu->timer);
}

static inline void qedu_clear_waiting_irq(struct qedu_device *edu)
{
	clear_bit(QEDU_STATE_IRQ_WAIT, &edu->state);
	clear_bit(QEDU_STATE_IRQ_OK, &edu->state);
	clear_bit(QEDU_STATE_IRQ_TIMEDOUT, &edu->state);
}
#endif
