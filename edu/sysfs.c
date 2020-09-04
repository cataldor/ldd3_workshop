// SPDX-License-Identifier: GPL-2.0-only
#include <linux/device.h>
#include <linux/io.h>
#include <linux/limits.h>
#include <linux/pci.h>
#include <linux/sysfs.h>

#include "hw.h"
#include "state.h"
#include "sysfs.h"

static ssize_t version_show(struct device_driver *dev, char *page)
{
	ssize_t size;
	const struct qedu_device *edu = pci_get_drvdata(qedu_dev);

	size = scnprintf(page, PAGE_SIZE, "major %u minor %u\n",
	    QEDU_MAJOR_VERSION(edu->id), QEDU_MINOR_VERSION(edu->id));
	return size;
}

static DRIVER_ATTR_RO(version);

static ssize_t inverse_show(struct device_driver *dev, char *page)
{
	u32 val;
	ssize_t size;
	unsigned long flags;
	struct qedu_device *edu = pci_get_drvdata(qedu_dev);

	spin_lock_irqsave(&edu->lock, flags);
	val = readl(edu->io_base + QEDU_INVERSE_REG);
	spin_unlock_irqrestore(&edu->lock, flags);

	size = scnprintf(page, PAGE_SIZE, "%x\n", val);
	return size;
}

static ssize_t inverse_store(struct device_driver *dev, const char *page,
    size_t count)
{
	u64 val;
	unsigned long flags;
	struct qedu_device *edu = pci_get_drvdata(qedu_dev);

	sscanf(page, "%llx", &val);
	if (val > UINT_MAX)
		return -EINVAL;

	spin_lock_irqsave(&edu->lock, flags);
	writel(val, edu->io_base + QEDU_INVERSE_REG);
	spin_unlock_irqrestore(&edu->lock, flags);
	return count;
}

static DRIVER_ATTR_RW(inverse);

static ssize_t test_irq_show(struct device_driver *dev, char *page)
{
	ssize_t size;
	unsigned long flags;
	struct qedu_device *edu = pci_get_drvdata(qedu_dev);

	spin_lock_irqsave(&edu->lock, flags);
	if (qedu_is_shutting_down(edu)) {
		size = -ECANCELED;
		goto end;
	}
	if (qedu_is_waiting_irq(edu)) {
		size = -EBUSY;
		goto end;
	}
	/*
	 * bits 0 and 2 of INTR_STATUS are used interally by edu.
	 * We use bit 1
	 */
	writel(BIT_MASK(1), edu->io_base + QEDU_INTR_RAISE_REG);
	qedu_set_waiting_irq(edu);
	while (!qedu_is_irq_done(edu)) {
		DEFINE_WAIT(wait_e);

		spin_unlock_irqrestore(&edu->lock, flags);
		prepare_to_wait(&edu->irq_q, &wait_e, TASK_INTERRUPTIBLE);
		if (!__qedu_is_irq_done(edu))
			schedule();
		finish_wait(&edu->irq_q, &wait_e);
		if (signal_pending(current))
			return -ERESTARTSYS;
		spin_lock_irqsave(&edu->lock, flags);
	}

	if (test_bit(QEDU_STATE_IRQ_OK, &edu->state))
		size = scnprintf(page, PAGE_SIZE, "IRQ OK\n");
	else
		size = scnprintf(page, PAGE_SIZE, "IRQ FAIL %lx\n", edu->state);

	qedu_clear_waiting_irq(edu);
end:
	spin_unlock_irqrestore(&edu->lock, flags);
	return size;
}

static DRIVER_ATTR_RO(test_irq);

int qedu_sysfs_create_entries(struct qedu_device *edu)
{
	int ret;
	struct device_driver *drv = &edu->pci_dev->driver->driver;

	ret = driver_create_file(drv, &driver_attr_version);
	if (ret)
		goto fail_version;

	ret = driver_create_file(drv, &driver_attr_inverse);
	if (ret)
		goto fail_inverse;

	ret = driver_create_file(drv, &driver_attr_test_irq);
	if (ret)
		goto fail_test_irq;

	return 0;
fail_test_irq:
	driver_remove_file(drv, &driver_attr_inverse);
fail_inverse:
	driver_remove_file(drv, &driver_attr_version);
fail_version:
	return ret;
}

void qedu_sysfs_remove_entries(struct qedu_device *edu)
{
	struct device_driver *drv = &edu->pci_dev->driver->driver;

	driver_remove_file(drv, &driver_attr_test_irq);
	driver_remove_file(drv, &driver_attr_inverse);
	driver_remove_file(drv, &driver_attr_version);
}
