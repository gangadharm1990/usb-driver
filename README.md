# usb-driver
usb driver to get device details, implemaentation of below ATAPI commands
1. INQUIRY command to get the device parameters information
2. TEST UNIT READY command to check if device is ready to receive medium access ATAPI commands
3. READ CAPACITY command provides a mechanism for host computer to request information regarding USB device capacity
4. READ_10 command requests the USB device to transfer data to the host computer from specified block and up to specified length
5. WRITE_10 command requests the USB device to transfer data from the host computer to the USB device
