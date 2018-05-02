#ifndef _USB_CORE_H_
#define _USB_CORE_H_
struct usbdev_private {
	struct usb_device *udev;
	unsigned char class;
	unsigned char subclass;
	unsigned char protocol;
	unsigned char ep_in; /*in endpoint*/
	unsigned char ep_out;/*out endpoint*/
	unsigned char max_lun;/*Count of Maximum Logical units*/
	unsigned int capacity; /*Capacity of pen drive*/
};

struct usb_cmd_info{
	unsigned char cmd;
	unsigned char lun;
	int cbw_data_xfer_len;
	int lba;
};

void decode_csw(unsigned char *data, unsigned int len);
void decode_inquiry_response(unsigned char *data, unsigned int len);
int usbdev_prepare_cmd_request(unsigned char *buff, int len, struct usb_cmd_info *cmd_info);
void decode_request_sense_response(unsigned char *data, unsigned int len);
int usbdev_send_read_10(int lba, int xfer_len,unsigned char *buffer);
int usbdev_send_write_10(int lba,int xfer_len,unsigned char *buffer);
#define TRACE(arg...) printk(arg)
#define LOG_MSG(arg...) printk(arg)

#define CBW_LEN 15
#define CBWCB_LEN 31
#define CSW_LEN 13
#define LUN 0
#define INQUIRY 0x12
#define INQUIRY_CMD_LEN 6
#define INQUIRY_RSP_LEN 36
#define TEST_UNIT_READY 0x00
#define TEST_UNIT_READY_CMD_LEN 6
#define READ_CAPACITY 0x25
#define READ_CAPACITY_CMD_LEN 6
#define READ_CAPACITY_RSP_LEN 8
#define READ_10 0x28
#define READ_10_CMD_LEN 10
#define USBDEV_SECTOR_SIZE 512
#define WRITE_10 0x2A
#define WRITE_10_CMD_LEN 10
#define REQUEST_SENSE 0X03
#define REQUEST_SENSE_CMD_LEN 9
#define REQUEST_SENSE_RSP_LEN 36
#endif
