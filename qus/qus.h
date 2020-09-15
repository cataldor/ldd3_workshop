#ifndef __QUS_H_
#define __QUS_H_

#include <linux/kref.h>

struct qemu_usb_storage {
	struct usb_device	*dev;
	struct usb_interface	*intf;
	struct urb		*urb;
	u8			*buf;
	size_t			 buf_len;
	u8			 bulk_in_endpoint;
	u8			 bulk_out_endpoint;
	struct kref		 kref;
};
#endif
