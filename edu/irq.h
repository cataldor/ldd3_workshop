#ifndef __QEDU_IRQ_H_
#define __QEDU_IRQ_H_

#include <linux/interrupt.h>

#include "hw.h"

irqreturn_t qedu_handle_irq(int irq, void *v);
irqreturn_t qedu_thr_handle_irq(int irq, void *v);
void qedu_timeout(struct timer_list *tl);

#endif
