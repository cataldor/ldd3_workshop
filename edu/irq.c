#include <linux/io.h>
#include <linux/sched.h>
#include <linux/pci.h>

#include "irq.h"

static bool __qedu_is_irq_raised(u32 intr_status)
{
	return !!(intr_status & 7);
}

static void __qedu_handle_irq(struct qedu_device *edu, u32 intr_status)
	__must_hold(&edu->lock)
{
	del_timer(&edu->timer);
	set_bit(QEDU_STATE_IRQ_OK, &edu->state);
	writel(intr_status, edu->io_base + QEDU_INTR_ACK_REG);
	wake_up_interruptible_sync(&edu->irq_q);
}

irqreturn_t qedu_handle_irq(int irq, void *v)
{
	unsigned long flags;
	struct qedu_device *edu = v;
	const u32 intr_status = readl(edu->io_base + QEDU_INTR_STATUS_REG);

	if (!__qedu_is_irq_raised(intr_status))
		return IRQ_NONE;

	if (spin_trylock_irqsave(&edu->lock, flags)) {
		__qedu_handle_irq(edu, intr_status);
		spin_unlock_irqrestore(&edu->lock, flags);
		return IRQ_HANDLED;
	} else
		return IRQ_WAKE_THREAD;
}

irqreturn_t qedu_thr_handle_irq(int irq, void *v)
{
	unsigned long flags;
	struct qedu_device *edu = v;
	const u32 intr_status = readl(edu->io_base + QEDU_INTR_STATUS_REG);

	spin_lock_irqsave(&edu->lock, flags);
	__qedu_handle_irq(edu, intr_status);
	spin_unlock_irqrestore(&edu->lock, flags);
	return IRQ_HANDLED;
}

static inline int __qedu_is_irq_set(u32 status, u32 dma_status)
{
	return (status & QEDU_STATUS_REG_IRQ_REQ) ||
	       (dma_status & QEDU_DMA_CMD_IRQ_REQ);
}

static inline int __qedu_is_busy_irq(u32 status, u32 dma_status)
{
	return (status & QEDU_STATUS_REG_BUSY) ||
		(dma_status & QEDU_DMA_CMD_IN_TRANSFER);
}

void qedu_timeout(struct timer_list *tl)
{
	u32 status;
	u32 dma_status;
	u32 intr_status;
	unsigned long flags;
	struct qedu_device *edu = from_timer(edu, tl, timer);

	spin_lock_irqsave(&edu->lock, flags);
	intr_status = readl(edu->io_base + QEDU_INTR_STATUS_REG);
	if (__qedu_is_irq_raised(intr_status))
		goto spin_unlock;

	status = readl(edu->io_base + QEDU_STATUS_REG);
	dma_status = readl(edu->io_base + QEDU_DMA_CMD_REG);
	if (!__qedu_is_irq_set(status, dma_status)) {
		dev_alert(&edu->pci_dev->dev, "[timeout:%u] no IRQ bit set\n",
		    edu->timeout/HZ);
		goto set_timeout;
	}
	if (__qedu_is_busy_irq(status, dma_status)) {
		dev_alert(&edu->pci_dev->dev, "[timeout:%u] qedu is still busy\n",
		    edu->timeout/HZ);
		mod_timer(&edu->timer, jiffies + edu->timeout);
		goto spin_unlock;
	}

set_timeout:
	set_bit(QEDU_STATE_IRQ_TIMEDOUT, &edu->state);
	wake_up_interruptible_sync(&edu->irq_q);
spin_unlock:
	spin_unlock_irqrestore(&edu->lock, flags);
}
