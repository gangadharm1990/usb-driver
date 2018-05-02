#include<linux/kernel.h>
#include<linux/module.h>
#include "usb_core.h"

void decode_csw(unsigned char *data, unsigned int len)
{
	if(len != CSW_LEN)
	{
		printk("Invalid CSW length\n");
		return;
	}

	printk("####### DECODING CSW ######\n");
	printk(KERN_INFO "%c",data[0]);
	printk(KERN_INFO "%c",data[1]);
	printk(KERN_INFO "%c",data[2]);
	printk(KERN_INFO "%c",data[3]);
	printk(KERN_INFO "Tag: %d\n", *(int *)(data+4));
	printk(KERN_INFO "Data Residue: %d\n", *(int *)(data+8));
	switch(data[12])
	{
		case 0x00: 
			printk("Command Passed\n");
			break;
		case 0x01:
			printk("Command Failed\n");
			break;
		case 0x02:
			printk("phase Error\n");
			break;
		default:
			printk("Unknown status : %d\n",data[12]);
	}
	return;	
	
}
void decode_inquiry_response(unsigned char *data, unsigned int len)
{
	unsigned char peripheral_dev_type;
	unsigned char rmb;
	unsigned char additional_len;
	int i;
	if(len != INQUIRY_RSP_LEN)
	{
		printk("Invalid INQUIRY response length\n");
		return;
	}
	peripheral_dev_type = data[0] & 0x1f;
	switch(peripheral_dev_type)
	{
		case 0: 
			printk(KERN_INFO "SBC Direct-access device\n");
			break;
		case 0x05:
			printk(KERN_INFO "CD-ROM device\n");
			break;
		case 0x07:
			printk(KERN_INFO "Optical memory device\n");
			break;
		case 0x0e:
			printk(KERN_INFO "RBC Direct-access device\n");
			break;
		default:
			printk(KERN_INFO "Out ofs scope\n");
			
	}
	rmb = data[1];
	if(rmb == 0x80)
		printk(KERN_INFO "Medium is removable\n");
	else if(rmb == 0)
		printk(KERN_INFO "Medium is not removable\n");
	else
		printk(KERN_INFO "Erroneous RMB\n");
	additional_len = data[4];
	printk(KERN_INFO "Additional Length : %d\n",additional_len);
	printk(KERN_INFO "Vendor:");
	for(i=8;i<16;i++)
	{
		printk(KERN_INFO "%c",data[i]);
	}
	printk("\n");
	for(i=16;i<32;i++)
		printk(KERN_INFO "%c",data[i]);
	printk(KERN_INFO "\n");
	printk(KERN_INFO "Product revision level: ");
	for(i=32;i<36;i++)
		printk(KERN_INFO "%c",data[i]);
	printk(KERN_INFO "\n");
	
	printk(KERN_INFO "Product: %s\n",data);
	return;
}

void decode_request_sense_response(unsigned char *data, unsigned int len)
{
	int asc,ascq,sense_key;
	asc = data[12];
	ascq = data[13];
	sense_key = data[2] & 0x0f;
	printk(KERN_INFO "\n asc : %d, ascq:%d,sense_key:%d \n",asc,ascq,sense_key);
	return;
}
