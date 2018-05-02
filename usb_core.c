#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/usb.h>
#include<linux/slab.h>
#include "usb_core.h"
#include "usb_bulk.h"
#define USB_PEN_DRIVE_VID 0x058f
#define USB_PEN_DRIVE_PID 0x6387
#define USB_KINGSTON_VID 0x0951
#define USB_KINGSTON_DT_PID 0x1602
#define USB_KINGSTON_4G_PID 0x1607

#define DIR_WRITE 1
#define DIR_READ 2

#define MAX_TRY 3

struct usbdev_private *p_usb_dev_info;



static void usbdev_wakeup_on_completion(struct urb *urb)
{
	struct completion *urb_done_ptr;
	TRACE("ENTER: usbdev_wakeup_on_completion\n");
	urb_done_ptr = (struct completion *)urb->context;
	complete(urb_done_ptr);
	return;
}


static int usbdev_bulk_raw(int dir, struct usbdev_private* p_usbdev,unsigned char *data, unsigned int len)
{
	int pipe, result, len_done, halt_clearing=0;
	struct completion *urb_done = NULL;
	struct urb *p_urb_request;
	struct usbdev_private *p_usb;
	p_usb = p_usbdev;
	unsigned char *dma_buf;
	urb_done = kmalloc(sizeof(struct completion),GFP_KERNEL);
	if(urb_done)
		init_completion(urb_done);
	else
	{
		printk("urb_done is fail\n");
		return -1;
	}
	if(dir == DIR_READ)
		pipe = usb_rcvbulkpipe(p_usb->udev,p_usb->ep_in);
	else if(dir == DIR_WRITE)	
		pipe = usb_sndbulkpipe(p_usb->udev,p_usb->ep_out);
	else
	{
		printk("USB_INVALID_REQ\n");
		return -1;
	}

	p_urb_request = usb_alloc_urb(0,GFP_KERNEL);
	if(p_urb_request == NULL)
	{
		printk("usb_alloc_urb failed\n");
		kfree(urb_done);
		return -1;
	}

	/*Allocating usb dma memory*/
	dma_buf = usb_alloc_coherent(p_usb->udev,len,GFP_KERNEL,&p_urb_request->transfer_dma);
	if(dma_buf == NULL)
	{
		printk("DMA buffer allocation failed\n");
		usb_free_urb(p_urb_request);
		kfree(urb_done);
	}
	else
		memset(dma_buf,0,len);
	if(dir == DIR_WRITE)
		memcpy(dma_buf,data,len);
	usb_fill_bulk_urb(p_urb_request,p_usb->udev,pipe,dma_buf,len,usbdev_wakeup_on_completion,urb_done);
	p_urb_request->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	result = usb_submit_urb(p_urb_request, GFP_KERNEL);
	if(result)
	{
		LOG_MSG("usb_submit_urb failed\n");
		usb_free_urb(p_urb_request);
		kfree(urb_done);
		return result;
	}

	/*wait for request completion */
	wait_for_completion(urb_done);
	kfree(urb_done);
	len_done = p_urb_request->actual_length;
	result = p_urb_request->status;
	if(dir == DIR_READ)
		memcpy(data,dma_buf,len_done);
	/*free usb dma memory*/
	usb_free_coherent(p_urb_request->dev,p_urb_request->transfer_buffer_length,p_urb_request->transfer_buffer,p_urb_request->transfer_dma);
	usb_free_urb(p_urb_request);
	/*error handling*/
	if(result){
		if(result == -ETIMEDOUT)
			LOG_MSG("usbdev_bulk_raw: timeout\n");
		else if(result == -ENOENT)
			LOG_MSG("usbdev_bulk_raw: transfer aborted\n");
		else if(result == -EPIPE){
			LOG_MSG("usbdev_bulk_raw: pipe stalled\n");
			if(usb_clear_halt(p_usb->udev,pipe)<0)
				LOG_MSG("halt could not be cleared\n");
		}
		return -1;
	}
	if(len_done != len) /*This is not really an error*/
		return -1;
	return 0;
}



int usbdev_bulk_data_out(struct usbdev_private* p_usbdev, unsigned char *src, unsigned int len)
{
	int status;
	TRACE("ENTER: usbdev_bulk_data_out\n");
	if(len == 0)
		return 0;
	status  = usbdev_bulk_raw(DIR_WRITE, p_usbdev,src, len);
	TRACE("EXIT: usbdev_bulk_data_out\n");
	return status;
}

int usbdev_bulk_data_in(struct usbdev_private* p_usbdev, unsigned char *dest, unsigned int len)
{
	int status;
	TRACE("ENTER: usbdev_bulk_data_in\n");
	if(len == 0)
		return 0;
	status = usbdev_bulk_raw(DIR_READ, p_usbdev,dest,len);
	TRACE("EXIT: usbdev_bulk_data_in\n");
	return status;
}



static int reset_device(struct usb_device *udev)
{
	int result = 0, pipe = usb_sndctrlpipe(udev,0);
	result = usb_control_msg(udev,pipe,0xff,USB_DIR_OUT|USB_TYPE_CLASS|USB_RECIP_INTERFACE,0,0,NULL,0,2*HZ);
	if(result < 0)
	{
		printk("Device reset failed (%d)\n",result);
		return -1;
	}
	printk("Device reset successfully\n");
	return 0;

}

static int get_max_lun(struct usb_device *udev)
{
	int pipe, result=0;
	unsigned char data = 0;
	pipe = usb_rcvctrlpipe(udev,0);
	result = usb_control_msg(udev,pipe,0xfe,USB_DIR_IN|USB_TYPE_CLASS|USB_RECIP_INTERFACE,0,0,&data,sizeof(data),HZ);
	if(result == 1)
		return data;
	printk("get_max_lun failed.. result = %d\n",result);
	return -1;
}

int usbdev_send_test_unit_ready(void)
{
	struct usbdev_private *p_usbdev = p_usb_dev_info;
	unsigned char cbw_buf[CBWCB_LEN] = {0}, csw_buf[CSW_LEN] = {0};
	int result;
	struct usb_cmd_info req_info;
	memset(&req_info,0,sizeof(req_info));
	req_info.cmd = TEST_UNIT_READY;
	req_info.lun = LUN;
	if(usbdev_prepare_cmd_request(cbw_buf,CBWCB_LEN,&req_info)<0)
	{
		LOG_MSG("usbdev_prepare_cmd_request failed\n");
		return -1;
	}
	/*send command*/
	result = usbdev_bulk_data_out(p_usbdev, cbw_buf,CBWCB_LEN);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_out failed\n");
		return -1;
	}
	/*read command status*/
	result = usbdev_bulk_data_in(p_usbdev,csw_buf,CSW_LEN);
	if(result != 0)
	{
		LOG_MSG("usbdev_bulk_data_in failed\n");
		return -1;
	}
	result = csw_buf[12] ? -1 : 0;
	if(result)
		printk(KERN_INFO "TEST_UNIT_READY_STATUS : %d\n",csw_buf[12]);
	return result;
}

int usbdev_request_sense(void)
{
	struct usbdev_private *p_usbdev = p_usb_dev_info;
	unsigned char cbw_buf[CBWCB_LEN] = {0}, csw_buf[0x24] = {0};
	int result;
	struct usb_cmd_info req_info;
	memset(&req_info,0,sizeof(req_info));
	req_info.cmd = REQUEST_SENSE;
	req_info.lun = LUN;
	req_info.cbw_data_xfer_len = REQUEST_SENSE_RSP_LEN;
	if(usbdev_prepare_cmd_request(cbw_buf,CBWCB_LEN,&req_info)<0)
	{
		LOG_MSG("usbdev_prepare_cmd_request failed\n");
		return -1;
	}
	/*send command*/
	result = usbdev_bulk_data_out(p_usbdev,cbw_buf,CBWCB_LEN);
	if(result != 0)
	{
		LOG_MSG("usbdev_bulk_data_out failed\n");
		return -1;
	}
	
	result = usbdev_bulk_data_in(p_usbdev,csw_buf,req_info.cbw_data_xfer_len);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_in failed\n");
		return -1;
	}
	else
		decode_request_sense_response(csw_buf,req_info.cbw_data_xfer_len);

	/*read command status*/
	result = usbdev_bulk_data_in(p_usbdev,csw_buf,CSW_LEN);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_in failed\n");
		return -1;
	}
	else
		decode_csw(csw_buf,CSW_LEN);
	result = csw_buf[12] ? -1 : 0;
	if(result)
		printk("REQUEST_SENSE STATUS : %d\n",csw_buf[12]);
	
	return result;
}



int usbdev_send_inquiry(void)
{
	struct usbdev_private *p_usbdev =p_usb_dev_info;
	unsigned char cbw_buf[CBWCB_LEN] = {0},csw_buf[0x24] = {0};
	int result;
	struct usb_cmd_info req_info;
	memset(&req_info,0,sizeof(req_info));
	req_info.cmd = INQUIRY;
	req_info.lun = LUN;
	req_info.cbw_data_xfer_len = INQUIRY_RSP_LEN;
	if(usbdev_prepare_cmd_request(cbw_buf,CBWCB_LEN,&req_info) < 0)
	{
		LOG_MSG("usbdev_prepare_cmd_request failed\n");
		return -1;
	}
	/*send command*/
	result = usbdev_bulk_data_out(p_usbdev,cbw_buf,CBWCB_LEN);
	if(result != 0)
	{
		LOG_MSG("usbdev_bulk_data_out failed\n");
		return -1;
	}
	/*read command response*/
	result = usbdev_bulk_data_in(p_usbdev,csw_buf,req_info.cbw_data_xfer_len);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_in failed\n");
		return -1;
	}
	else
		decode_inquiry_response(csw_buf,req_info.cbw_data_xfer_len);
	/*read command status*/
	result = usbdev_bulk_data_in(p_usbdev,csw_buf,CSW_LEN);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_in failed\n");
		return -1;
	}
	else
		decode_csw(csw_buf,CSW_LEN);
	result = csw_buf[12] ? -1 : 0;
	if(result)
		printk("INQUIRY_STATUS : %d\n",csw_buf[12]);
	return result;
}


static int device_info(struct usb_interface *interface)
{
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;
	unsigned char epAddr, epAttr;
	/*Set up the endpoint information*/
	/*check out the endpoints*/
	iface_desc = &interface->altsetting[0];
	printk("No of Alt Settings = %d\n",interface->num_altsetting);
	printk("No of endpoints = %d\n",iface_desc->desc.bNumEndpoints);
	for(i=0; i<iface_desc->desc.bNumEndpoints;i++)
	{	
		endpoint = &iface_desc->endpoint[i].desc;
		epAddr = endpoint->bEndpointAddress;
		epAttr = endpoint->bmAttributes;
		if((epAttr & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)
		{
			if((epAddr & 0x80) == USB_DIR_IN)
			{
				LOG_MSG("found a bulk in endpoint: %d\n",i);
				p_usb_dev_info->ep_in = endpoint->bEndpointAddress;
			}
			if((epAddr & 0x80) == USB_DIR_OUT)
			{
				LOG_MSG("found a bulk out endpoint: %d\n",i);
				p_usb_dev_info->ep_out = endpoint->bEndpointAddress;
			}
		}
	}
	
	/*set up device information*/

	p_usb_dev_info->class = iface_desc->desc.bInterfaceClass;
	p_usb_dev_info->subclass= iface_desc->desc.bInterfaceSubClass;
	p_usb_dev_info->protocol = iface_desc->desc.bInterfaceProtocol;
	LOG_MSG("class : %d\n",p_usb_dev_info->class);
	LOG_MSG("subclass: %d\n",p_usb_dev_info->subclass);
	LOG_MSG("protocol: %d\n",p_usb_dev_info->protocol);

	return 0;
}


int usbdev_read_capacity(void)
{
	struct usbdev_private *p_usbdev = p_usb_dev_info;
	unsigned char cbw_buf[CBWCB_LEN] = {0}, csw_buf[CSW_LEN] = {0};
	int result;
	struct usb_cmd_info req_info;
	unsigned int llba,block_len;	
	memset(&req_info,0,sizeof(req_info));
	req_info.cmd = READ_CAPACITY;
	req_info.lun = LUN;
	req_info.cbw_data_xfer_len = READ_CAPACITY_RSP_LEN;
	if(usbdev_prepare_cmd_request(cbw_buf,CBWCB_LEN,&req_info) < 0)
	{
		LOG_MSG("usbdev_prepare_cmd_request failed\n");
		return -1;
	}
	/*send command*/
	result = usbdev_bulk_data_out(p_usbdev,cbw_buf,CBWCB_LEN);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_out failed\n");
		return -1;
	}
	/*read command response*/
	result = usbdev_bulk_data_in(p_usbdev,csw_buf,req_info.cbw_data_xfer_len);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_in failed\n");
		return -1;
	}
	llba = (csw_buf[0] << 24) | (csw_buf[1] << 16) | (csw_buf[2] << 8)| (csw_buf[3]);
	block_len = (csw_buf[4] << 24) | (csw_buf[5] << 16) | (csw_buf[6] << 8)| (csw_buf[7]);
	/*read command status*/
	result = usbdev_bulk_data_in(p_usbdev,csw_buf,CSW_LEN);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_in failed\n");
		return -1;
	}
	result = csw_buf[12] ?-1:0;
	if(result)
		printk(KERN_INFO "READ_CAPACITY_STATUS : %d\n",csw_buf[12]);
	else
	{
		printk(KERN_INFO "Last Logical Block Address: %x\n",llba);
		printk(KERN_INFO "Block length in Bytes: %d\n",block_len);
		p_usbdev->capacity = llba+1;
		printk(KERN_INFO "capacity is %d\n",p_usbdev->capacity);
	}
	return result;
}

int usbdev_send_read_10(int lba, int xfer_len,unsigned char *buffer)
{
	struct usbdev_private *p_usbdev = p_usb_dev_info;
	unsigned char cbw_buf[CBWCB_LEN] = {0}, csw_buf[CSW_LEN] = {0};
	int result;
	struct usb_cmd_info  req_info;
	LOG_MSG("READ_10: sector: 0x%x, xfer_len: %d(0x%x sectors)\n", lba,xfer_len,xfer_len/512);
	memset(&req_info,0,sizeof(req_info));
	req_info.cmd = READ_10;
	req_info.lun = LUN;
	req_info.cbw_data_xfer_len = xfer_len;
	req_info.lba = lba;
	if(usbdev_prepare_cmd_request(cbw_buf,CBWCB_LEN,&req_info) < 0)
	{
		LOG_MSG("usbdev_prepare_cmd_request failed\n");
		return -1;
	}
	/*send command*/
	result = usbdev_bulk_data_out(p_usbdev,cbw_buf,CBWCB_LEN);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_out failed\n");
		return -1;
	}
	/*read command response*/
	result = usbdev_bulk_data_in(p_usbdev,buffer,req_info.cbw_data_xfer_len);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_in failed\n");
		return -1;
	}
	/*read command status*/
	result = usbdev_bulk_data_in(p_usbdev,csw_buf,CSW_LEN);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_in failed\n");
		return -1;
	}
	result = csw_buf[12] ?-1:0;
	if(result)
		printk(KERN_INFO "READ_10_STATUS : %d\n",csw_buf[12]);
	return result;
}


int usbdev_send_write_10(int lba, int xfer_len,unsigned char *buffer)
{
	struct usbdev_private *p_usbdev = p_usb_dev_info;
	unsigned char cbw_buf[CBWCB_LEN] = {0}, csw_buf[CSW_LEN] = {0};
	int result;
	struct usb_cmd_info  req_info;
	memset(&req_info,0,sizeof(req_info));
	req_info.cmd = WRITE_10;
	req_info.lun = LUN;
	req_info.cbw_data_xfer_len = xfer_len;
	req_info.lba = lba;
	if(usbdev_prepare_cmd_request(cbw_buf,CBWCB_LEN,&req_info) < 0)
	{
		LOG_MSG("usbdev_prepare_cmd_request failed\n");
		return -1;
	}
	/*send command*/
	result = usbdev_bulk_data_out(p_usbdev,cbw_buf,CBWCB_LEN);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_out failed\n");
		return -1;
	}
	/*write data*/
	result = usbdev_bulk_data_out(p_usbdev,buffer,req_info.cbw_data_xfer_len);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_out failed\n");
		return -1;
	}
	/*read command status*/
	result = usbdev_bulk_data_in(p_usbdev,csw_buf,CSW_LEN);
	if(result!=0)
	{
		LOG_MSG("usbdev_bulk_data_in failed\n");
		return -1;
	}
	result = csw_buf[12] ?-1:0;
	if(result)
		printk(KERN_INFO "WRITE_10_STATUS : %d\n",csw_buf[12]);
	return result;
	
	
}
static int usbdev_probe(struct usb_interface *interface,const struct usb_device_id *id)
{
	TRACE("<1>usbdev_probe ENTRY \n");
	int try = 0;
	p_usb_dev_info = kmalloc(sizeof(struct usbdev_private),GFP_KERNEL);
	if(!p_usb_dev_info)
		return -1;
	memset(p_usb_dev_info,0,sizeof(struct usbdev_private));
	p_usb_dev_info->udev = interface_to_usbdev(interface);
	usb_get_dev(p_usb_dev_info->udev);

	if(device_info(interface))
	{
		kfree(p_usb_dev_info);
		return -1;
	}

	/*reset USB*/
	reset_device(p_usb_dev_info->udev);
	/*Get count of maximum logical units*/
	p_usb_dev_info->max_lun = get_max_lun(p_usb_dev_info->udev);
	printk(KERN_INFO "max_lun is %d\n",p_usb_dev_info->max_lun);
	if(usbdev_send_inquiry() < 0)
	{
		LOG_MSG("USB_SEND_ENQUIRY FAILED\n");
		return -1;
	}
	else	
		LOG_MSG("USB_SEND_INQUIRY OK\n");
	do{
            if(usbdev_send_test_unit_ready()<0)
		{
			LOG_MSG("USB_SEND_TEST_UNIT_READY FAILED\n");
		}
		else
		{
		LOG_MSG("USB_SEND_TEST_UNIT_READY OK\n");
		break;
		}
	try++;
	}while(try < MAX_TRY);
	if(try == MAX_TRY)
	{
		LOG_MSG("Device not ready..giving up\n");
		return -1;
	}
	if(usbdev_read_capacity() < 0)
	{
		LOG_MSG("USB_READ_CAPACITY FAILED\n");
		//usbdev_do_sense(TEST_UNIT_READY);
		return -1;
	}
	else
		LOG_MSG("USB_READ_CAPACITY Ok\n");
	if(usbdev_request_sense() < 0)
	{
		LOG_MSG("Send Request sense failed\n");
		return -1;
	}
	
	//init_usb_bulk(p_usb_dev_info->capacity);

	return 0;
}


static void usbdev_disconnect(struct usb_interface *interface)
{
	TRACE("usbdev_disconnect called\n");
	usb_put_dev(p_usb_dev_info->udev);
	cleanup_usb_bulk();
	return;
}

/*table of devices that work with this driver*/
static struct usb_device_id usbdev_table[] = {
{USB_DEVICE(USB_PEN_DRIVE_VID,USB_PEN_DRIVE_PID)},
{USB_DEVICE(USB_KINGSTON_VID,USB_KINGSTON_DT_PID)},
{USB_DEVICE(USB_KINGSTON_VID,USB_KINGSTON_4G_PID)},
{}

};

static struct usb_driver usbdev_driver = {
	name : "usbdev",
	probe: usbdev_probe,
	disconnect: usbdev_disconnect,
	id_table: usbdev_table
};

int init_module(void)
{
	usb_register(&usbdev_driver);
	return 0;
}

void cleanup_module(void)
{
	usb_deregister(&usbdev_driver);
	return;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gangadara Reddy <mgreddy424_at_gmail_dot_com>");
MODULE_DESCRIPTION("USB Pen Drive Registration Driver");
