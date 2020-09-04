// SPDX-License-Identifier: GPL-2.0-only
/*
 * QEMU edu device driver
 * Written by: Rodrigo Cataldo <cadorecataldo@gmail.com>
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#include "irq.h"
#include "hw.h"
#include "state.h"
#include "sysfs.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Rodrigo Cataldo");

struct pci_dev *qedu_dev;

static struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(QEDU_VENDOR_ID, QEDU_DEVICE_ID), },
	{ 0, },
};

static int edu_dma_mask = QEDU_DEFAULT_DMA_MASK;
module_param(edu_dma_mask, int, 0644);
MODULE_PARM_DESC(edu_dma_mask, "DMA address mask (default: 28 bits)");
static bool edu_use_msi = 1;
module_param(edu_use_msi, bool, 0644);
MODULE_PARM_DESC(edu_use_msi, "Use MSI for interrupts (default: 1)");

static void __qedu_remove(struct pci_dev *dev, struct qedu_device *edu,
		unsigned long *flags)
	__must_hold(&edu->lock)
{

	lockdep_assert_held(&edu->lock);
	/* let everyone know that we are shutting down */
	set_bit(QEDU_STATE_SHUTDOWN, &edu->state);
	spin_unlock_irqrestore(&edu->lock, *flags);

	/* cannot be holding the spinlock */
	del_timer_sync(&edu->timer);

	spin_lock_irqsave(&edu->lock, *flags);
	if (!qedu_is_irq_done(edu))
		dev_err(&dev->dev,
		    "(i) timer was not set or (ii) IRQ scheduled while shutting down\n");

	/* wait for after-process IRQ */
	if (test_bit(QEDU_STATE_IRQ_WAIT, &edu->state)) {
		spin_unlock_irqrestore(&edu->lock, *flags);
		dev_info(&dev->dev, "waiting for IRQ process to finish\n");
		wait_event_interruptible_timeout(edu->irq_q, 0, HZ);
		spin_lock_irqsave(&edu->lock, *flags);
	}

	(void)free_irq(edu->irq, edu);
	if (edu->use_msi)
		pci_free_irq_vectors(dev);
	pci_iounmap(dev, edu->io_base);	
	/*
	 * pci.rst:
	 * Conversely, drivers should call pci_release_region() AFTER
	 * calling pci_disable_device().
	 */
	pci_disable_device(dev);
	pci_release_regions(dev);
}

static void qedu_remove(struct pci_dev *dev)
{
	unsigned long flags;
	struct qedu_device *edu = pci_get_drvdata(dev);

	dev_dbg(&dev->dev, "edu_remove\n");
	if (edu == NULL)
		return;

	/* remove userspace entry points */
	qedu_sysfs_remove_entries(edu);
	spin_lock_irqsave(&edu->lock, flags);
	__qedu_remove(dev, edu, &flags);
	spin_unlock_irqrestore(&edu->lock, flags);
	kfree(edu);
}

static int qedu_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	unsigned long flags;
	int ret;
	struct qedu_device *edu;

	dev_dbg(&dev->dev, "qedu_probe\n");
	edu = kzalloc(sizeof(*edu), GFP_KERNEL);
	if (edu == NULL)
		return -ENOMEM;	

	spin_lock_init(&edu->lock);
	spin_lock_irqsave(&edu->lock, flags);
	init_waitqueue_head(&edu->irq_q);
	timer_setup(&edu->timer, qedu_timeout, 0);
	qedu_dev = dev;
	edu->pci_dev = dev;
	ret = pci_enable_device(dev);
	if (ret) {
		dev_err(&dev->dev, "pci_enable_device\n");
		goto fail_enable;
	}

	ret = pci_request_region(dev, 0, "edu_bar0");
	if (ret) {
		dev_err(&dev->dev, "pci_request_region for bar0\n");
		goto fail_request_region;
	}

	edu->io_base = pci_iomap(dev, 0, pci_resource_len(dev, 0));
	if (edu->io_base == NULL) {
		dev_err(&dev->dev, "pci_iomap\n");
		ret = -ENODEV;
		goto fail_iomap;
	}

	ret = pci_set_dma_mask(dev, DMA_BIT_MASK(edu_dma_mask));
	if (ret && edu_dma_mask != QEDU_DEFAULT_DMA_MASK) {
		dev_err(&dev->dev, "dma mask %d bits rejected; using default",
				edu_dma_mask);
		edu_dma_mask = QEDU_DEFAULT_DMA_MASK;
		ret = pci_set_dma_mask(dev, DMA_BIT_MASK(edu_dma_mask));
		if (ret) {
			dev_err(&dev->dev, "pci_set_dma_mask\n");
			goto fail;
		}
	}

	ret = pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(edu_dma_mask));
	if (ret) {
		dev_err(&dev->dev, "pci_set_consistent_dma_mask\n");
		goto fail;
	}

	pci_set_master(dev);
	pci_set_drvdata(dev, edu);

	/* XXX: setup msi, dma capabilities */
	if (edu_use_msi) {
		ret = pci_alloc_irq_vectors(dev, 1, 1, PCI_IRQ_MSI);
		if (ret < 0) {
			edu_use_msi = 0;
			dev_err(&dev->dev, "pci_alloc_irq_vectors %d\n", ret);
			edu->irq = dev->irq;
		} else {
			ret = pci_irq_vector(dev, 0);
			if (ret < 0) {
				dev_err(&dev->dev, "pci_irq_vector %d\n", ret);
				goto fail_irq;
			}
			edu->irq = ret;
		}
	} else
		edu->irq = dev->irq;

	ret = request_threaded_irq(edu->irq, qedu_handle_irq,
	    qedu_thr_handle_irq, (edu_use_msi ? 0 : IRQF_SHARED), "qedu", edu);
	if (ret) {
		dev_err(&dev->dev, "irq allocation failed\n");
		goto fail_irq;
	}

	edu->id = readl(edu->io_base + QEDU_MMIO_ID_REG);
	if (!QEDU_IS_ID(edu->id)) {
		dev_err(&dev->dev, "edu id check failed: %x\n", edu->id);
		goto fail;
	}

	ret = qedu_sysfs_create_entries(edu);
	if (ret)
		goto fail;

	dev_info(&dev->dev, "[hw v%u.%u, dma mask %d MSI %d len %llu mb]\n",
	    QEDU_MAJOR_VERSION(edu->id), QEDU_MINOR_VERSION(edu->id),
	    edu_dma_mask, edu_use_msi, pci_resource_len(dev, 0)/1024/1024);

	edu->use_msi = edu_use_msi;
	spin_unlock_irqrestore(&edu->lock, flags);
	return 0;

fail:
	__qedu_remove(dev, edu, &flags);
	goto end;

fail_irq:
	pci_iounmap(dev, edu->io_base);	
fail_iomap:
	/* see comment on __qedmu_remove */
	pci_disable_device(dev);
	pci_release_regions(dev);
fail_enable:
	goto end;

fail_request_region:
	pci_disable_device(dev);
	goto end;

end:
	spin_unlock_irqrestore(&edu->lock, flags);
	kfree(edu);
	qedu_dev = NULL;
	return ret;
}

static struct pci_driver edu_driver = {
	.name		= "qedu",
	.id_table 	= pci_ids,
	.probe		= qedu_probe,
	.remove		= qedu_remove,
};

static int __init edu_init(void)
{
	return pci_register_driver(&edu_driver);
}

static void __exit edu_end(void)
{
	pci_unregister_driver(&edu_driver);
}

module_init(edu_init);
module_exit(edu_end);
