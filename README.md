## Smartlink firmware dumper for Linux

Firmware dumper for MP3 players with a chip labeled as Jointbees MP3, the player shows a version that starts with `yp3_`. The manufacturer of this chip is "Shenzhen Shenju Technology". YP3 is written as 云P3 in Chinese.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, USE AT YOUR OWN RISK!

#### Chip identification

The device has two modes:

1. Card reader mode, when connected to USB with a card inserted.
2. Bootloader mode, if you turn off the device and hold down the boot key when connecting to USB.

* The boot key can be any key, it varies from device to device. You will have to search by trial and error.
* If the device does not have a physical power switch, you can exit bootloader mode by long pressing the power key.

##### SL6801

Card reader: id = `301a:2801`, inquiry = `SMTLINK CARDREADER 1.00`, serial = `20201111000001`  
Bootloader mode: id = `301a:2800`, inquiry = `SMTLINK DEVICE 2.00`, serial = `20201111000001`  

##### SL6806

Card reader: id = `301a:2800`, inquiry = `SSTLINK DDVICE 2.02` (with typos), serial = `20221008000002`  
Bootloader mode: id = `301a:2800`, inquiry = `SMTLINK DEVICE 2.00`, serial = `20220320000001`  

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

* Where 2M is the expected length of flash in bytes (may be more or less, SL6806 has 4MB).
* An example payload is [here](payload) (you can read the chip's ROM with it).

#### Commands

`chip <6801|6806>` - select chip.  
`serial` - print the serial number of the USB device (`libusb` mode only). The serial number actually indicates the chip version.  

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
`read_mem <addr> <size> <output_file>` - read memory (uses tiny payload, can work with SL6801 in card reader mode).  

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

