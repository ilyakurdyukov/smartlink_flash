#include "sl6801_sys.h"

static int dummy_cb(void) { return 0; }

static int cmd_read_mem(uint8_t *cdb) {
	uint32_t len = *(uint32_t*)(cdb + 2);
	uint8_t *addr = *(uint8_t**)(cdb + 6);
	if (len > USB_RET_BUF_SIZE) return USB_RET_ERROR;
	memcpy(USB_RET_BUF, addr, len);
	USB_RET_SIZE(len);
	return USB_RET_DATA;
}

static int scsi_cb(uint8_t *cdb) {
	int cmd = cdb[1];
	if (cdb[0] != 0xc0) return USB_RET_ERROR;
	switch (cmd) {
	case 64: return cmd_read_mem(cdb);
	}
	return USB_RET_ERROR;
}

void scsi_main(void) {
	static const usb_userfn_t data =
		{ NULL, NULL, dummy_cb, NULL, NULL, NULL, scsi_cb, NULL, NULL };

	usb_set_userfn(&data);
	USB_RET_SIZE(0);
}

