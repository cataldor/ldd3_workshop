// SPDX-License-Identifier: GPL-2.0-only
#include <linux/device.h>
#include <linux/io.h>
#include <linux/limits.h>
#include <linux/pci.h>
#include <linux/sysfs.h>

#include "hw.h"
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
	ssize_t size;
	u32 val;
	const struct qedu_device *edu = pci_get_drvdata(qedu_dev);

	val = readl(edu->io_base + QEDU_INVERSE_REG);
	size = scnprintf(page, PAGE_SIZE, "%x\n", val);
	return size;
}

static ssize_t inverse_store(struct device_driver *dev, const char *page,
    size_t count)
{
	u64 val;
	const struct qedu_device *edu = pci_get_drvdata(qedu_dev);

	sscanf(page, "%llx", &val);
	if (val > UINT_MAX)
		return -EINVAL;

	writel(val, edu->io_base + QEDU_INVERSE_REG);
	return count;
}

static DRIVER_ATTR_RW(inverse);

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

	return 0;
fail_inverse:
	driver_remove_file(drv, &driver_attr_version);
fail_version:
	return ret;
}

void qedu_sysfs_remove_entries(struct qedu_device *edu)
{
	struct device_driver *drv = &edu->pci_dev->driver->driver;

	driver_remove_file(drv, &driver_attr_inverse);
	driver_remove_file(drv, &driver_attr_version);
}
