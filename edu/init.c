// SPDX-License-Identifier: GPL-2.0-only
/*
 * QEMU edu device driver
 * Written by: Rodrigo Cataldo <cadorecataldo@gmail.com>
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#include "hw.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Rodrigo Cataldo");

static struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID), },
	{ 0, },
};

static int edu_dma_mask = EDU_DEFAULT_DMA_MASK;
module_param(edu_dma_mask, int, 0644);
MODULE_PARM_DESC(edu_dma_mask, "DMA address mask (default: 28 bits)");

static void edu_remove(struct pci_dev *dev)
{
	struct edu_device *edu = pci_get_drvdata(dev);

	dev_dbg(&dev->dev, "edu_remove\n");
	if (edu == NULL)
		return;

	pci_iounmap(dev, edu->io_base);	
	pci_release_regions(dev);
	pci_disable_device(dev);
	kfree(edu);
}

static int edu_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int ret;
	struct edu_device *edu;

	dev_dbg(&dev->dev, "edu_probe\n");
	edu = kzalloc(sizeof(*edu), GFP_KERNEL);
	if (edu == NULL)
		return -ENOMEM;	

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
	if (ret && edu_dma_mask != EDU_DEFAULT_DMA_MASK) {
		dev_err(&dev->dev, "dma mask %d bits rejected; using default",
				edu_dma_mask);
		edu_dma_mask = EDU_DEFAULT_DMA_MASK;
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

	/* XXX: setup interrupts, dma capabilities */

	edu->id = readl(edu->io_base + EDU_MMIO_ID_REG);
	if (!EDU_IS_ID(edu->id)) {
		dev_err(&dev->dev, "edu id check failed: %x\n", edu->id);
		goto fail;
	}

	dev_info(&dev->dev, "[hw version %u.%u, dma mask %d len %llu mb]\n",
	    EDU_MAJOR_VERSION(edu->id), EDU_MINOR_VERSION(edu->id),
	    edu_dma_mask, pci_resource_len(dev, 0)/1024/1024);

	return 0;

fail:
	pci_iounmap(dev, edu->io_base);	
fail_iomap:
	pci_release_regions(dev);
fail_request_region:
	pci_disable_device(dev);
fail_enable:
	kfree(edu);
	return ret;

}

static struct pci_driver edu_driver = {
	.name		= "edu_driver",
	.id_table 	= pci_ids,
	.probe		= edu_probe,
	.remove		= edu_remove,
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
