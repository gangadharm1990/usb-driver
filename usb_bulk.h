#ifndef _USB_BULK_H_
#define _USB_BULK_H_
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#define DEVICE_NAME "USB_CARD_READER"
#define MAJOR_NR 125
struct blkdev_private {
	struct workqueue_struct *usbdevQ;
	spinlock_t lock;
	struct request_queue *queue;
	struct gendisk *gd;
};
struct usbdev_work {
	struct work_struct work;
	struct request *req;
};

void cleanup_usb_bulk(void);
int init_usb_bulk(unsigned int capacity);
#endif
