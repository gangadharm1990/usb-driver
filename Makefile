#obj-m += usb_core.o
obj-m := usb.o
usb-objs := usb_core.o usb_transport.o usb_debug.o usb_bulk.o 

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
