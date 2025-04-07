## Smartlink firmware dumper for Linux

Filmware dumper for MP3 players with a chip labeled as Jointbees MP3, the player shows a version that starts with `yp3_`. The manufacturer of this chip is "Shenzhen Shenju Technology". YP3 is written as 云P3 in Chinese.

When connected with SD card inserted it shows as `301a:2801 SMTLINK CARDREADER`. The specific key on the device is the boot key, when you turn off and connect while holding that key, it shows as `301a:2800 SMTLINK DEVICE`.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, USE AT YOUR OWN RISK!

### Build

There are two options:

1. Using `libusb` for Linux and Windows (MSYS2):  
Use `make`, `libusb/libusb-dev` packages must be installed.

* For Windows users - please read how to install a [driver](https://github.com/libusb/libusb/wiki/Windows#driver-installation) for `libusb`.

2. Using the USB serial, **Linux only**:  
Use `make LIBUSB=0`.
If you're using this mode, you must initialize the USB serial driver before using the tool (every boot):
```
$ sudo modprobe ftdi_sio
$ echo 301a 2800 | sudo tee /sys/bus/usb-serial/drivers/generic/new_id
```

* On Linux you must run the tool with `sudo`, unless you are using special udev rules (see below).

### Instructions

To make a dump from bootloader mode, you must first use the `init` command:
```
$ sudo ./smtlink_dump  init  flash_id  read_flash 0 2M dump.bin
```

You can dump when connected as a card reader, but do not use the `init` command (it will hang quickly), and specify the device ID in card reader mode:
```
$ sudo ./smtlink_dump --id 301a:2801  flash_id  read_flash 0 2M dump.bin
```

* Where 2MB is the expected length of flash in bytes (may be more or less).
* An example payload is [here](payload) (you can read the chip's ROM with it).

#### Commands

`serial` - print the serial number of the USB device (`libusb` mode only). The serial number actually indicates the chip version (`20201111000001` means `sl6801`, `20220320000001` means `sl6806`).  

Basic commands supported by the chip's boot ROM:

`inquiry` - print SCSI device information.  
`init` - use first when using bootloader mode, do not use in card reader mode.  
`flash_id` - read some 32-bit ID.  
`reset` - reboot the device (doesn't really work).  
`read_flash <addr> <size> <output_file>`  
`check_flash <addr> <size>` - CRC16 of the specified range.  
`write_mem <addr> <file_offset> <size> <input_file>` - zero size means until the end of the file.  
`exec <addr>` - execute code at the specified address.  
`exec_ret <addr> <ret_size>` - execute the code and read the result.  
`simple_exec <addr> <file>` - equivalent to `write_mem <addr> 0 0 <file> exec <addr+1>`.  
`cmd_ret <cmd> <len_arg> <addr_arg> <ret_size>` - run the specified command and read the result.  
`read_mem <addr> <size> <output_file>` - read memory, can work in card reader mode (uses tiny payload, only tested on SL6801).  

The commands below require loading the payload binary that comes with the tool (using the command `simple_exec 0x820000 payload.bin`).

* Don't load the payload in card reader mode, the result will be unpredictable (most likely the device will hang).

`read_mem2 <addr> <size> <output_file>`  

#### Using the tool without sudo

If you create `/etc/udev/rules.d/80-smtlink.rules` with these lines:
```
# Smartlink
SUBSYSTEMS=="usb", ATTRS{idVendor}=="301a", ATTRS{idProduct}=="2800", MODE="0666", TAG+="uaccess"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="301a", ATTRS{idProduct}=="2801", MODE="0666", TAG+="uaccess"
```
...then you can run `smtlink_dump` without root privileges.

