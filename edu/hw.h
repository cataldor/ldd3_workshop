// SPDX-License-Identifier: GPL-2.0-only
#ifndef __QEDU_HW_H_
#define __QEDU_HW_H_

#include <linux/wait.h>
#include <linux/timer.h>
/*
 * EDU_ is taken by broadcast driver (brcmnand)
 * use QEDU_ instead.
 */
#define QEDU_VENDOR_ID	0x1234
#define QEDU_DEVICE_ID	0x11e8

#define QEDU_DEFAULT_DMA_MASK	28

/* MMIO area */
/* RO */
#define QEDU_MMIO_ID_REG		0x00
#define QEDU_INVERSE_REG		0x04
#define QEDU_FACTORIAL_REG		0x08
#define QEDU_STATUS_REG			0x20
/* RO */
#define QEDU_INTR_STATUS_REG		0x24
/* WO */
#define QEDU_INTR_RAISE_REG		0x60
#define QEDU_INTR_ACK_REG		0x64
#define QEDU_DMA_SRC_REG		0x80
#define QEDU_DMA_DST_REG		0x88
#define QEDU_DMA_COUNT_REG		0x90
#define QEDU_DMA_CMD_REG		0x98

/* for QEDU_STATUS_REG */
#define QEDU_STATUS_REG_BUSY		0x01
#define QEDU_STATUS_REG_IRQ_REQ		0x80

/* for QEDU_DMA_CMD_REG */
#define QEDU_DMA_CMD_IN_TRANSFER	0x01
#define QEDU_DMA_CMD_IRQ_REQ		0x04

#define QEDU_MAJOR_VERSION(val)	(val >> 24)
#define QEDU_MINOR_VERSION(val)	((val << 8) >> 24)
#define QEDU_IS_ID(val)	((val & 0xfffff) == 0x000ed)

#define QEDU_STATE_IRQ_WAIT	0
#define	QEDU_STATE_IRQ_OK	1
#define	QEDU_STATE_IRQ_TIMEDOUT 2
#define QEDU_STATE_SHUTDOWN	3

/*
 * used by edu/sysfs.c for driver private data.
 */
extern struct pci_dev *qedu_dev;
struct qedu_device {
	spinlock_t	lock;
	void __iomem 	*io_base;
	struct pci_dev *pci_dev;
	u32	id;
	u32	irq;
	u32	timeout;
	unsigned long	state;
	bool	use_msi;
	struct timer_list timer;
	wait_queue_head_t irq_q;
};

#endif
