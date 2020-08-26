// SPDX-License-Identifier: GPL-2.0-only
#ifndef __EDU_HW_H_
#define __EDU_HW_H_

#define EDU_VENDOR_ID	0x1234
#define EDU_DEVICE_ID	0x11e8

#define EDU_DEFAULT_DMA_MASK	28

/* MMIO area */
/* RO */
#define EDU_MMIO_ID_REG		0x00
#define EDU_INVERSE_REG		0x04
#define EDU_FACTORIAL_REG	0x08
#define EDU_STATUS_REG		0x20	
/* RO */
#define EDU_INTR_STATUS_REG	0x24
/* WO */
#define EDU_INTR_RAISE_REG	0x60
#define EDU_INTR_ACK_REG	0x64
#define EDU_DMA_SRC_REG		0x80
#define EDU_DMA_DST_REG		0x88
#define EDU_DMA_COUNT_REG	0x90
#define EDU_DMA_CMD_REG		0x98

#define EDU_MAJOR_VERSION(val)	(val >> 24)
#define EDU_MINOR_VERSION(val)	((val << 8) >> 24)
#define EDU_IS_ID(val)	((val & 0x0000fffff) == 0x000000ed)

struct edu_device {
	u32	id;
	void __iomem 	*io_base;
};

#endif
