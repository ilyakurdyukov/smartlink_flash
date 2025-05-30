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
	uint32_t unknown[4];
} usb_userfn_t;

#define DEF_ROM_FN(addr, ret, name, args) \
	static ret (* const name) args = (ret (*) args)(addr + 1);

DEF_ROM_FN(0x640, void, printf, (const char *fmt, ...))
DEF_ROM_FN(0xb82, char*, strcpy, (char *dst, const char *src))
DEF_ROM_FN(0xb96, char*, strncpy, (char *dst, const char *src, size_t n))
DEF_ROM_FN(0xbbc, char*, strchr, (const char *src, int ch))
DEF_ROM_FN(0xbd8, char*, strrchr, (const char *src, int ch))
DEF_ROM_FN(0xbf2, size_t, strlen, (const char *src))
DEF_ROM_FN(0xc02, size_t, strnlen, (const char *src, size_t n))
DEF_ROM_FN(0xc16, int, strcmp, (const void *s1, const void *s2))
DEF_ROM_FN(0xc30, int, strncmp, (const void *s1, const void *s2, size_t n))
DEF_ROM_FN(0xc4e, char*, strcat, (char *dst, const char *src))
DEF_ROM_FN(0xc64, char*, strncat, (char *dst, const char *src, size_t n))
DEF_ROM_FN(0xc8e, void*, memcpy, (void *dst, const void *src, size_t n))
DEF_ROM_FN(0xd0e, void*, memset, (void *dst, int val, size_t n))
DEF_ROM_FN(0xd82, void*, memmove, (void *dst, const void *src, size_t n))
DEF_ROM_FN(0xdb8, int, memcmp, (const void *s1, const void *s2, size_t n))
DEF_ROM_FN(0xdd4, void*, malloc, (size_t n))
DEF_ROM_FN(0xdd8, void, free, (void *p))
DEF_ROM_FN(0x5b70, void, usb_set_userfn, (const usb_userfn_t*))

enum {
	USB_RET_ERROR = -1,
	USB_RET_DATA = 0,
	USB_RET_NODATA = 1
};

#define USB_DESC 0x800df8

#define USB_RET_SIZE(n) \
	*(volatile uint16_t*)(USB_DESC + 6) = n;
#define USB_RET_BUF *(uint8_t**)(USB_DESC + 8)
#define USB_RET_BUF_SIZE 64

