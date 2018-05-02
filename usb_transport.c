#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/slab.h>
#include<linux/highmem.h>
#include "usb_core.h"

static int usb_cmd_num = 0;
int usbdev_prepare_cmd_request (unsigned char *buff, int len, struct usb_cmd_info *cmd_info)
{
	int command_len = 0;
	if(len < CBWCB_LEN)
	{
		LOG_MSG("Request buffer too short\n");
		return -1;
	}
	else
		memset(buff,0,len);

	/*dCBWSignature*/
	buff[0] = 'U';
	buff[1] = 'S';
	buff[2] = 'B';
	buff[3] = 'C';
	
	/*dCBWTag*/
	memcpy(buff+4, &usb_cmd_num,4);
	usb_cmd_num++;

	/*dCBWDataTransferLength*/
	memcpy(buff+8, &cmd_info->cbw_data_xfer_len,4);
	
	/*bmCBWFlags must be filled later depending upon request*/
	buff[13] = cmd_info->lun & 0x0f;
	
	/*encode CB*/
	switch(cmd_info->cmd)
	{
		case INQUIRY:
			buff[12] = 0x80;/*direction*/
			buff[14] = INQUIRY_CMD_LEN&0x1f;
			/*cb*/
			buff[CBW_LEN+0] = INQUIRY;
			buff[CBW_LEN+1] = (cmd_info->lun << 5)&0xe0;
			buff[CBW_LEN+4] = cmd_info->cbw_data_xfer_len;
			command_len = CBWCB_LEN;
			break;
		case TEST_UNIT_READY:
			buff[14] = TEST_UNIT_READY_CMD_LEN & 0x1f;
			/*cb*/
			buff[CBW_LEN+0] = TEST_UNIT_READY;
			buff[CBW_LEN+1] = (cmd_info->lun << 5)&0xe0;
			command_len = CBWCB_LEN;
			break;
		case READ_CAPACITY:
			buff[12] = 0x80;
			buff[14] = READ_CAPACITY_CMD_LEN & 0x1f;
			/*cb*/
			buff[CBW_LEN+0] = READ_CAPACITY;
			buff[CBW_LEN+1] = (cmd_info->lun << 5)&0xe0;
			command_len = CBWCB_LEN;
			break;
		case READ_10:
			buff[12] = 0x80; /*direction*/
			buff[14] = READ_10_CMD_LEN & 0xf;
			/*cb*/
			buff[CBW_LEN+0] = READ_10;
			buff[CBW_LEN+1] = (cmd_info->lun << 5)&0xe0;
			buff[CBW_LEN+2] = (unsigned char)(cmd_info->lba >> 24);
			buff[CBW_LEN+3] = (unsigned char)(cmd_info->lba >> 16);
			buff[CBW_LEN+4] = (unsigned char)(cmd_info->lba >> 8);
			buff[CBW_LEN+5] = (unsigned char)(cmd_info->lba);
			buff[CBW_LEN+7] = (unsigned char) ((cmd_info->cbw_data_xfer_len)/USBDEV_SECTOR_SIZE >> 8);
			buff[CBW_LEN+8] = (unsigned char) (cmd_info->cbw_data_xfer_len)/USBDEV_SECTOR_SIZE ;
			command_len = CBWCB_LEN;
			break;
		case WRITE_10:
			buff[12] = 0x00; /*direction*/
			buff[14] = WRITE_10_CMD_LEN & 0x1f;
			/*cb*/
			buff[CBW_LEN+0] = WRITE_10;
			buff[CBW_LEN+1] = (cmd_info->lun << 5)&0xe0;
			buff[CBW_LEN+2] = (unsigned char)(cmd_info->lba >> 24);
			buff[CBW_LEN+3] = (unsigned char)(cmd_info->lba >> 16);
			buff[CBW_LEN+4] = (unsigned char)(cmd_info->lba >> 8);
			buff[CBW_LEN+5] = (unsigned char)(cmd_info->lba);
			buff[CBW_LEN+7] = (unsigned char) ((cmd_info->cbw_data_xfer_len)/USBDEV_SECTOR_SIZE >> 8);
			buff[CBW_LEN+8] = (unsigned char) (cmd_info->cbw_data_xfer_len)/USBDEV_SECTOR_SIZE ;
			command_len = CBWCB_LEN;
			break;

		case REQUEST_SENSE:
			buff[12] = 0x80;
			buff[14] = REQUEST_SENSE_CMD_LEN & 0x1f;
			/*cb*/
			buff[CBW_LEN+0] = REQUEST_SENSE;
			buff[CBW_LEN+1] = (cmd_info->lun << 5)&0xe0;
			buff[CBW_LEN+4] = cmd_info->cbw_data_xfer_len;
			command_len = CBWCB_LEN;
			break;
			
		default:
			command_len = -1;
			LOG_MSG("Command not supported\n");
			return -1;
	}	
	return command_len;		
}


