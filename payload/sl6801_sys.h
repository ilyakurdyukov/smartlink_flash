#include <stdint.h>
#include <stddef.h>

typedef struct {
	int (*cb0)(void);
	int (*cb1)(void);
	int (*cb2)(void);
	int (*cb3)(void);
	int (*cb4)(void);
	int (*cb5)(void);
	int (*scsi_cb)(uint8_t*);
	const uint8_t *scsi_id;
	int (*cb8)(void);
} usb_userfn_t;

#define DEF_ROM_FN(addr, ret, name, args) \
	static ret (* const name) args = (void*)(addr + 1);

DEF_ROM_FN(0x2b0, void, printf, (const char *fmt, ...))
DEF_ROM_FN(0xb2a, void*, memcpy, (void *dst, const void *src, size_t n))
DEF_ROM_FN(0xbaa, void*, memset, (void *dst, int val, size_t n))
DEF_ROM_FN(0xc54, int, memcmp, (const void *s1, const void *s2, size_t n))
DEF_ROM_FN(0xc70, void*, malloc, (size_t n))
DEF_ROM_FN(0xc74, void, free, (void *p))
DEF_ROM_FN(0x5914, void, usb_set_userfn, (const usb_userfn_t*))

enum {
	USB_RET_ERROR = -1,
	USB_RET_DATA = 0,
	USB_RET_NODATA = 1
};

#define USB_DESC 0x800de8

#define USB_RET_SIZE(n) \
	*(volatile uint16_t*)(USB_DESC + 6) = n;
#define USB_RET_BUF (uint8_t*)(USB_DESC + 8)
#define USB_RET_BUF_SIZE 64

