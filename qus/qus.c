// SPDX-License-Identifier: GPL-2.0
#include "qus.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

MODULE_LICENSE("GPL v2");

/*
 * this driver is based on usb-skeleton.c and Documentation/driver-api/usb
 */
#define QUS_VENDOR_ID	0x46f4
#define QUS_PRODUCT_ID	0x0001

static const struct usb_device_id qus_table[] = {
	{ USB_DEVICE(QUS_VENDOR_ID, QUS_PRODUCT_ID) },
	{ }
};
MODULE_DEVICE_TABLE(usb, qus_table);

/* same as usb-skeleton; real driver would need to be different */
#define QUS_MINOR_BASE	192

#define to_qus_kref(k)	container_of(k, struct qemu_usb_storage, kref)
static void qus_del(struct kref *kref)
{
	struct qemu_usb_storage *qus = to_qus_kref(kref);

	usb_free_urb(qus->urb);
	usb_put_intf(qus->intf);
	usb_put_dev(qus->dev);
	kfree(qus->buf);
	kfree(qus);
}

static const struct file_operations qus_fops = {
	.owner	= THIS_MODULE,
};

static struct usb_class_driver qus_class = {
	.name 	= "qus%d",
	.fops = &qus_fops,
	.minor_base = QUS_MINOR_BASE,
};


static int qus_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	int ret;
	struct usb_endpoint_descriptor *bulk_in, *bulk_out;
	struct qemu_usb_storage *qus;

	qus = kzalloc(sizeof(*qus), GFP_KERNEL);
	if (qus == NULL)
		return -ENOMEM;

	kref_init(&qus->kref);

	qus->dev = usb_get_dev(interface_to_usbdev(intf));
	qus->intf = usb_get_intf(intf);

	/* XXX: control endpoint */
	/* interrupt endpoints not usually needed for mass storage */
	ret = usb_find_common_endpoints(intf->cur_altsetting,
	    &bulk_in, &bulk_out, NULL, NULL);
	if (ret) {
		dev_err(&intf->dev, "usb_find_common_endpoints\n");
		goto error;
	}

	qus->bulk_in_endpoint = bulk_in->bEndpointAddress;
	qus->buf_len = usb_endpoint_maxp(bulk_in);
	qus->buf = kmalloc(qus->buf_len, GFP_KERNEL);
	if (qus->buf == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	qus->urb = usb_alloc_urb(0, GFP_KERNEL);
	if (qus->urb == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	qus->bulk_out_endpoint = bulk_out->bEndpointAddress;

	usb_set_intfdata(intf, qus);

	ret = usb_register_dev(intf, &qus_class);
	if (ret) {
		dev_err(&intf->dev, "usb_register_dev\n");
		usb_set_intfdata(intf, NULL);
		goto error;
	}

	dev_info(&intf->dev, "qus%d: bulk size in %u out %u\n",
	    intf->minor, usb_endpoint_maxp(bulk_in), usb_endpoint_maxp(bulk_out));
	return 0;
error:
	kref_put(&qus->kref, qus_del);
	return ret;
}

static void qus_disconnect(struct usb_interface *intf)
{
	struct qemu_usb_storage *qus;
	const int minor = intf->minor;

	qus = usb_get_intfdata(intf);
	usb_set_intfdata(intf, NULL);

	usb_deregister_dev(intf, &qus_class);

	/* wait for it */
	usb_kill_urb(qus->urb);

	kref_put(&qus->kref, qus_del);
	dev_info(&intf->dev, "qus%d disconnected\n", minor);
}

static struct usb_driver qus_driver = {
	.name		=	"qemu_usb_storage",
	.probe  	=	qus_probe,
	.disconnect 	= 	qus_disconnect,
	.id_table 	= 	qus_table,
	.supports_autosuspend = 0,
};

/* replaces module_init/module_exit */
module_usb_driver(qus_driver);
