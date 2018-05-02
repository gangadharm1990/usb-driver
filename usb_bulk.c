#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/blk_types.h>
#include <linux/bio.h>
#include "usb_core.h"
#include "usb_bulk.h"

static int blkdev_open(struct inode *inode,struct file *filep)
{
	TRACE("blkdev_open called\n");
	return 0;
}
static int blkdev_release(struct inode *inode,struct file *filep)
{
	TRACE("blkdev_release called\n");
	return 0;
}
static struct block_device_operations blkdev_ops = 
{
	owner : THIS_MODULE,
	open  : blkdev_open,
	release : blkdev_release
};

static struct blkdev_private *p_blkdev = NULL;
typedef struct request_queue request_queue_t;

int __do_usbdev_data_transfer(struct request *current_request)
{
	int result=-1, num_sect=0;
	unsigned int direction = rq_data_dir(current_request);
	struct bio *bio;
	__rq_for_each_bio(bio,current_request)
	{
		int i;
		struct bio_vec bvec;
		struct bvec_iter biter;
		sector_t sector = bio->bi_iter.bi_sector;
		bio_for_each_segment(bvec,bio,biter)
		{
			char *buffer = NULL;
			int total_len = blk_rq_cur_sectors(current_request)*USBDEV_SECTOR_SIZE;
			unsigned char *tmp_buffer = kmalloc(total_len,GFP_ATOMIC);
			if(!tmp_buffer)
			{
				printk("buffer allocation failed\n");
				return 0;
			}
			if(direction == READ)
			{
				result = usbdev_send_read_10(sector,total_len,tmp_buffer);
				buffer = __bio_kmap_atomic(bio,biter);
				memcpy(buffer,tmp_buffer,total_len);
				__bio_kunmap_atomic(buffer);
			}
			if(direction == WRITE)
			{
				buffer = __bio_kmap_atomic(bio,biter);
				memcpy(tmp_buffer,buffer,total_len);
				__bio_kunmap_atomic(buffer);
				result = usbdev_send_write_10(sector,total_len,tmp_buffer);
			}
			kfree(tmp_buffer);
			if(result < 0)
			{
				LOG_MSG("USBDEV_SEND_%s_10 FAILED\n", direction == READ ? "READ" : "WRITE");
			}
			else
			{
				LOG_MSG("USBDEV_SEND_%s_10 OK\n", direction == READ ? "READ" : "WRITE");
			}
			sector += blk_rq_cur_sectors(current_request);
		}
		num_sect += (bio->bi_iter.bi_size)/USBDEV_SECTOR_SIZE;
	}				
		
	return num_sect;
}

void do_usbdev_data_transfer(struct work_struct *work)
{
	struct usbdev_work *usb_work;
	struct request *current_request;
	int sectors_xferred=1;
	unsigned long flags;	
	usb_work = container_of(work,struct usbdev_work ,work);
	current_request = usb_work->req;
	LOG_MSG("do_usbdev_data_trasfer req = %p\n",current_request);
	sectors_xferred = __do_usbdev_data_transfer(current_request);
	spin_lock_irqsave(&p_blkdev->lock,flags);
	//if(!end_that_request_first(current_request,1,sectors_xferred))
	if(!(blk_end_request(current_request,1,(sectors_xferred*USBDEV_SECTOR_SIZE))))
	{
		LOG_MSG("req %p finished\n",current_request);
		//end_that_request_last(current_request,1);
	}
	spin_unlock_irqrestore(&p_blkdev->lock,flags);
	kfree(usb_work);
	return;


}
void usbdev_request(request_queue_t *q)
{
	struct request *req;
	struct usbdev_work *usb_work = NULL;
	while((req = blk_peek_request(q))!=NULL)
	{
		//if(!blk_fs_request(req))
		if(req->cmd_type != REQ_TYPE_FS)
		{
			//end_request(req,0);
			blk_end_request(req,-1,0);
			continue;
		}	
		usb_work = kmalloc(sizeof(struct usbdev_work),GFP_ATOMIC);
		if(!usb_work)
		{
			printk("Memory allocation failed\n");
			//end_request(req,0);
			blk_end_request(req,-1,0);
			continue;
		}
		LOG_MSG("deferring req = %p\n",req);
		usb_work->req = req;
		INIT_WORK(&usb_work->work,do_usbdev_data_transfer);
		queue_work(p_blkdev->usbdevQ,&usb_work->work);
		blk_start_request(req);
	}
	return;
}

int init_usb_bulk(unsigned int capacity)
{
	struct gendisk *usb_disk = NULL;
	p_blkdev = kmalloc(sizeof(struct blkdev_private),GFP_KERNEL);
	if(!p_blkdev)
	{
		printk(KERN_INFO "ENOMEM in %s at %d\n",__FUNCTION__,__LINE__);
		return 0;
	}
	memset(p_blkdev,0,sizeof(struct blkdev_private));
	p_blkdev->usbdevQ = create_workqueue("usbdevQ");
	if(!p_blkdev->usbdevQ)
	{
		printk(KERN_INFO "create work queue failed\n");
		goto err;
	}
	spin_lock_init(&p_blkdev->lock);
	p_blkdev->queue = blk_init_queue(usbdev_request,&p_blkdev->lock);
	usb_disk = p_blkdev->gd = alloc_disk(2);
	if(!usb_disk)
	{
		goto err2;
	}
	usb_disk->major = MAJOR_NR;
	usb_disk->first_minor = 0;
	usb_disk->fops = &blkdev_ops;
	usb_disk->queue = p_blkdev->queue;	
	usb_disk->private_data = p_blkdev;
	strcpy(usb_disk->disk_name,DEVICE_NAME);
	set_capacity(usb_disk,capacity);
	add_disk(usb_disk);
	LOG_MSG("registered block device\n");
	return 0;
err2:
	destroy_workqueue(p_blkdev->usbdevQ);
	LOG_MSG("allocating usb_disk failed\n");
	return 0;
err:
	kfree(p_blkdev);
	LOG_MSG("could not register block device\n");
	return 0;
}	

void cleanup_usb_bulk(void)
{
	struct gendisk *usb_disk = p_blkdev->gd;
	del_gendisk(usb_disk);
	blk_cleanup_queue(p_blkdev->queue);
	destroy_workqueue(p_blkdev->usbdevQ);
	kfree(p_blkdev);

}



