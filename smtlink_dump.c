/*
// Smartlink firmware dumper for Linux.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
*/

#define _GNU_SOURCE 1

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifndef LIBUSB_DETACH
/* detach the device from crappy kernel drivers */
#define LIBUSB_DETACH 1
#endif

#if USE_LIBUSB
#include <libusb-1.0/libusb.h>
#else
#include <termios.h>
#include <fcntl.h>
#include <poll.h>
#endif
#include <unistd.h>

static void print_mem(FILE *f, const uint8_t *buf, size_t len) {
	size_t i; int a, j, n;
	for (i = 0; i < len; i += 16) {
		n = len - i;
		if (n > 16) n = 16;
		for (j = 0; j < n; j++) fprintf(f, "%02x ", buf[i + j]);
		for (; j < 16; j++) fprintf(f, "   ");
		fprintf(f, " |");
		for (j = 0; j < n; j++) {
			a = buf[i + j];
			fprintf(f, "%c", a > 0x20 && a < 0x7f ? a : '.');
		}
		fprintf(f, "|\n");
	}
}

static void print_string(FILE *f, uint8_t *buf, size_t n) {
	size_t i; int a, b = 0;
	fprintf(f, "\"");
	for (i = 0; i < n; i++) {
		a = buf[i]; b = 0;
		switch (a) {
		case '"': case '\\': b = a; break;
		case 0: b = '0'; break;
		case '\b': b = 'b'; break;
		case '\t': b = 't'; break;
		case '\n': b = 'n'; break;
		case '\f': b = 'f'; break;
		case '\r': b = 'r'; break;
		}
		if (b) fprintf(f, "\\%c", b);
		else if (a >= 32 && a < 127) fprintf(f, "%c", a);
		else fprintf(f, "\\x%02x", a);
	}
	fprintf(f, "\"\n");
}

#define ERR_EXIT(...) \
	do { fprintf(stderr, __VA_ARGS__); exit(1); } while (0)

#define DBG_LOG(...) fprintf(stderr, __VA_ARGS__)

#define RECV_BUF_LEN 1024
#define TEMP_BUF_LEN (64 << 10)

typedef struct {
	uint8_t *recv_buf, *buf;
#if USE_LIBUSB
	libusb_device_handle *dev_handle;
	int endp_in, endp_out;
#else
	int serial;
#endif
	int flags, recv_len, recv_pos, nread;
	int verbose, timeout;
} usbio_t;

#if USE_LIBUSB
static void find_endpoints(libusb_device_handle *dev_handle, int result[2]) {
	int endp_in = -1, endp_out = -1;
	int i, k, err;
	//struct libusb_device_descriptor desc;
	struct libusb_config_descriptor *config;
	libusb_device *device = libusb_get_device(dev_handle);
	if (!device)
		ERR_EXIT("libusb_get_device failed\n");
	//if (libusb_get_device_descriptor(device, &desc) < 0)
	//	ERR_EXIT("libusb_get_device_descriptor failed");
	err = libusb_get_config_descriptor(device, 0, &config);
	if (err < 0)
		ERR_EXIT("libusb_get_config_descriptor failed : %s\n", libusb_error_name(err));

	for (k = 0; k < config->bNumInterfaces; k++) {
		const struct libusb_interface *interface;
		const struct libusb_interface_descriptor *interface_desc;
		int claim = 0;
		interface = config->interface + k;
		if (interface->num_altsetting < 1) continue;
		interface_desc = interface->altsetting + 0;
		for (i = 0; i < interface_desc->bNumEndpoints; i++) {
			const struct libusb_endpoint_descriptor *endpoint;
			endpoint = interface_desc->endpoint + i;
			if (endpoint->bmAttributes == 2) {
				int addr = endpoint->bEndpointAddress;
				err = 0;
				if (addr & 0x80) {
					if (endp_in >= 0) ERR_EXIT("more than one endp_in\n");
					endp_in = addr;
					claim = 1;
				} else {
					if (endp_out >= 0) ERR_EXIT("more than one endp_out\n");
					endp_out = addr;
					claim = 1;
				}
			}
		}
		if (claim) {
			i = interface_desc->bInterfaceNumber;
#if LIBUSB_DETACH
			err = libusb_kernel_driver_active(dev_handle, i);
			if (err > 0) {
				DBG_LOG("kernel driver is active, trying to detach\n");
				err = libusb_detach_kernel_driver(dev_handle, i);
				if (err < 0)
					ERR_EXIT("libusb_detach_kernel_driver failed : %s\n", libusb_error_name(err));
			}
#endif
			err = libusb_claim_interface(dev_handle, i);
			if (err < 0)
				ERR_EXIT("libusb_claim_interface failed : %s\n", libusb_error_name(err));
			break;
		}
	}
	if (endp_in < 0) ERR_EXIT("endp_in not found\n");
	if (endp_out < 0) ERR_EXIT("endp_out not found\n");
	libusb_free_config_descriptor(config);

	//DBG_LOG("USB endp_in=%02x, endp_out=%02x\n", endp_in, endp_out);

	result[0] = endp_in;
	result[1] = endp_out;
}
#else
static void init_serial(int serial) {
	struct termios tty = { 0 };

	// B921600
	cfsetispeed(&tty, B115200);
	cfsetospeed(&tty, B115200);

	tty.c_cflag = CS8 | CLOCAL | CREAD;
	tty.c_iflag = IGNPAR;
	tty.c_oflag = 0;
	tty.c_lflag = 0;

	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;

	tcflush(serial, TCIFLUSH);
	tcsetattr(serial, TCSANOW, &tty);
}
#endif

#if USE_LIBUSB
static usbio_t* usbio_init(libusb_device_handle *dev_handle, int flags) {
#else
static usbio_t* usbio_init(int serial, int flags) {
#endif
	uint8_t *p; usbio_t *io;

#if USE_LIBUSB
	int endpoints[2];
	find_endpoints(dev_handle, endpoints);
#else
	init_serial(serial);
	// fcntl(serial, F_SETFL, FNDELAY);
	tcflush(serial, TCIOFLUSH);
#endif

	p = (uint8_t*)malloc(sizeof(usbio_t) + RECV_BUF_LEN + TEMP_BUF_LEN);
	io = (usbio_t*)p; p += sizeof(usbio_t);
	if (!p) ERR_EXIT("malloc failed\n");
	io->flags = flags;
#if USE_LIBUSB
	io->dev_handle = dev_handle;
	io->endp_in = endpoints[0];
	io->endp_out = endpoints[1];
#else
	io->serial = serial;
#endif
	io->recv_len = 0;
	io->recv_pos = 0;
	io->recv_buf = p; p += RECV_BUF_LEN;
	io->buf = p;
	io->verbose = 0;
	io->timeout = 1000;
	return io;
}

static void usbio_free(usbio_t* io) {
	if (!io) return;
#if USE_LIBUSB
	libusb_close(io->dev_handle);
#else
	close(io->serial);
#endif
	free(io);
}

#define WRITE16_BE(p, a) do { \
  uint32_t __tmp = a; \
	((uint8_t*)(p))[0] = (uint8_t)(__tmp >> 8); \
	((uint8_t*)(p))[1] = (uint8_t)(a); \
} while (0)

#define WRITE32_LE(p, a) do { \
  uint32_t __tmp = a; \
	((uint8_t*)(p))[0] = (uint8_t)__tmp; \
	((uint8_t*)(p))[1] = (uint8_t)(__tmp >> 8); \
	((uint8_t*)(p))[2] = (uint8_t)(__tmp >> 16); \
	((uint8_t*)(p))[3] = (uint8_t)(__tmp >> 24); \
} while (0)

#define READ32_BE(p) ( \
	((uint8_t*)(p))[0] << 24 | \
	((uint8_t*)(p))[1] << 16 | \
	((uint8_t*)(p))[2] << 8 | \
	((uint8_t*)(p))[3])

#define READ16_LE(p) ( \
	((uint8_t*)(p))[1] << 8 | \
	((uint8_t*)(p))[0])

#define READ32_LE(p) ( \
	((uint8_t*)(p))[3] << 24 | \
	((uint8_t*)(p))[2] << 16 | \
	((uint8_t*)(p))[1] << 8 | \
	((uint8_t*)(p))[0])

static int usb_send(usbio_t *io, const void *data, int len) {
	const uint8_t *buf = (const uint8_t*)data;
	int ret;

	if (!buf) buf = io->buf;
	if (!len) ERR_EXIT("empty message\n");
	if (io->verbose >= 2) {
		DBG_LOG("send (%d):\n", len);
		print_mem(stderr, buf, len);
	}

#if USE_LIBUSB
	{
		int err = libusb_bulk_transfer(io->dev_handle,
				io->endp_out, (uint8_t*)buf, len, &ret, io->timeout);
		if (err < 0)
			ERR_EXIT("usb_send failed : %s\n", libusb_error_name(err));
	}
#else
	ret = write(io->serial, buf, len);
#endif
	if (ret != len)
		ERR_EXIT("usb_send failed (%d / %d)\n", ret, len);

#if !USE_LIBUSB
	tcdrain(io->serial);
	// usleep(1000);
#endif
	return ret;
}

static int usb_recv(usbio_t *io, int plen) {
	uint8_t *buf = io->buf;
	int a, pos, len, nread = 0;
	if (plen > TEMP_BUF_LEN)
		ERR_EXIT("target length too long\n");

	len = io->recv_len;
	pos = io->recv_pos;
	while (nread < plen) {
		if (pos >= len) {
#if USE_LIBUSB
			int err = libusb_bulk_transfer(io->dev_handle, io->endp_in, io->recv_buf, RECV_BUF_LEN, &len, io->timeout);
			if (err == LIBUSB_ERROR_NO_DEVICE)
				ERR_EXIT("connection closed\n");
			else if (err == LIBUSB_ERROR_TIMEOUT) break;
			else if (err < 0)
				ERR_EXIT("usb_recv failed : %s\n", libusb_error_name(err));
#else
			if (io->timeout >= 0) {
				struct pollfd fds = { 0 };
				fds.fd = io->serial;
				fds.events = POLLIN;
				a = poll(&fds, 1, io->timeout);
				if (a < 0) ERR_EXIT("poll failed, ret = %d\n", a);
				if (fds.revents & POLLHUP)
					ERR_EXIT("connection closed\n");
				if (!a) break;
			}
			len = read(io->serial, io->recv_buf, RECV_BUF_LEN);
#endif
			if (len < 0)
				ERR_EXIT("usb_recv failed, ret = %d\n", len);

			if (io->verbose >= 2) {
				DBG_LOG("recv (%d):\n", len);
				print_mem(stderr, io->recv_buf, len);
			}
			pos = 0;
			if (!len) break;
		}
		a = io->recv_buf[pos++];
		io->buf[nread++] = a;
	}
	io->recv_len = len;
	io->recv_pos = pos;
	io->nread = nread;
	return nread;
}

static uint8_t* loadfile(const char *fn, size_t *num) {
	size_t n, j = 0; uint8_t *buf = 0;
	FILE *fi = fopen(fn, "rb");
	if (fi) {
		fseek(fi, 0, SEEK_END);
		n = ftell(fi);
		if (n) {
			fseek(fi, 0, SEEK_SET);
			buf = (uint8_t*)malloc(n);
			if (buf) j = fread(buf, 1, n, fi);
		}
		fclose(fi);
	}
	if (num) *num = j;
	return buf;
}

#define USBC_SIG 0x43425355
#define USBS_SIG 0x53425355
#define USBC_LEN 31
#define USBS_LEN 13

#define CMD_INQUIRY 0x12

typedef struct {
	uint32_t sig;
	uint32_t tag;
	uint32_t data_len;
	uint8_t	flags;
	uint8_t	lun;
	uint8_t	cdb_len;
	uint8_t	cdb[16];
} usbc_cmd_t;

typedef struct {
	uint32_t sig;
	uint32_t tag;
	uint32_t residue;
	uint8_t	status;
} usbs_cmd_t;

static int flash_idx = 0;
static uint32_t scsi_tag = 1;

static int check_usbs(usbio_t *io, void *ptr) {
	usbs_cmd_t *usbs = (usbs_cmd_t*)(ptr ? ptr : io->buf);
	do {
		if (!ptr && usb_recv(io, USBS_LEN) != USBS_LEN) break;
		if (READ32_LE(&usbs->sig) != USBS_SIG) break;
		if (READ32_LE(&usbs->tag) != (int)scsi_tag++) break;
		return 0;
	} while (0);
	DBG_LOG("unexpected status\n");
	return 1;
}

static void smtlink_cmd(usbio_t *io, int cmd,
		uint32_t len, uint32_t addr, int recv, int data_len) {
	usbc_cmd_t usbc;
	WRITE32_LE(&usbc.sig, USBC_SIG);
	WRITE32_LE(&usbc.tag, scsi_tag);
	WRITE32_LE(&usbc.data_len, data_len);
	usbc.flags = recv << 7;
	usbc.lun = 0;
	usbc.cdb_len = 16;
	memset(usbc.cdb, 0, 16);
	usbc.cdb[0] = 0xc0 | flash_idx;
	usbc.cdb[1] = cmd;
	WRITE32_LE(usbc.cdb + 2, len);
	WRITE32_LE(usbc.cdb + 6, addr);
	usb_send(io, &usbc, USBC_LEN);
}

static void write_mem_buf(usbio_t *io,
		uint32_t addr, unsigned size, uint8_t *mem, unsigned step) {
	uint32_t i, n;

	for (i = 0; i < size; i += n) {
		n = size - i;
		if (n > step) n = step;
		smtlink_cmd(io, 9, n, addr + i, 0, n);
		usb_send(io, mem + i, n);
		if (check_usbs(io, NULL))
			ERR_EXIT("write_mem failed\n");
	}
}

static void write_mem(usbio_t *io,
		uint32_t addr, unsigned src_offs, unsigned src_size,
		const char *fn, unsigned step) {
	uint8_t *mem; size_t size = 0;
	mem = loadfile(fn, &size);
	if (!mem) ERR_EXIT("loadfile(\"%s\") failed\n", fn);
	if (size >> 32) ERR_EXIT("file too big\n");
	if (size < src_offs)
		ERR_EXIT("data outside the file\n");
	size -= src_offs;
	if (src_size) {
		if (size < src_size)
			ERR_EXIT("data outside the file\n");
		size = src_size;
	}
	write_mem_buf(io, addr, size, mem + src_offs, step);
	free(mem);
}

static unsigned dump_flash(usbio_t *io,
		uint32_t addr, uint32_t size, const char *fn, unsigned step) {
	unsigned i, n, n2;
	FILE *fo = fopen(fn, "wb");
	if (!fo) ERR_EXIT("fopen(wb) failed\n");

	for (i = 0; i < size; i += n) {
		n = size - i;
		if (n > step) n = step;
		smtlink_cmd(io, 7, n, addr + i, 1, n);
		n2 = n + USBS_LEN;
		if (usb_recv(io, n2) != (int)n2) {
			ERR_EXIT("unexpected length\n");
			break;
		}
		if (check_usbs(io, io->buf + n)) break;
		if (fwrite(io->buf, 1, n, fo) != n) 
			ERR_EXIT("fwrite failed\n");
	}
	DBG_LOG("dump_flash: 0x%08x, target: 0x%x, read: 0x%x\n", addr, size, i);
	fclose(fo);
	return i;
}

static uint64_t str_to_size(const char *str) {
	char *end; int shl = 0; uint64_t n;
	n = strtoull(str, &end, 0);
	if (*end) {
		if (!strcmp(end, "K")) shl = 10;
		else if (!strcmp(end, "M")) shl = 20;
		else if (!strcmp(end, "G")) shl = 30;
		else ERR_EXIT("unknown size suffix\n");
	}
	if (shl) {
		int64_t tmp = n;
		tmp >>= 63 - shl;
		if (tmp && ~tmp)
			ERR_EXIT("size overflow on multiply\n");
	}
	return n << shl;
}

#define REOPEN_FREQ 10

int main(int argc, char **argv) {
#if USE_LIBUSB
	libusb_device_handle *device;
#else
	int serial;
#endif
	usbio_t *io; int ret, i;
	int wait = 300 * REOPEN_FREQ;
	const char *tty = "/dev/ttyUSB0";
	int verbose = 0;
	uint32_t info[4] = { -1, -1, -1, -1 };
	// cartreader = 301a:2801, bootloader = 301a:2800
	int id_vendor = 0x301a, id_product = 0x2800;
	int blk_size = 1024;

#if USE_LIBUSB
	ret = libusb_init(NULL);
	if (ret < 0)
		ERR_EXIT("libusb_init failed: %s\n", libusb_error_name(ret));
#endif

	while (argc > 1) {
		if (!strcmp(argv[1], "--tty")) {
			if (argc <= 2) ERR_EXIT("bad option\n");
			tty = argv[2];
			argc -= 2; argv += 2;
		} else if (!strcmp(argv[1], "--id")) {
			if (argc <= 2) ERR_EXIT("bad option\n");
			char *end;
			id_vendor = strtol(argv[2], &end, 16);
			if (end != argv[2] + 4 || !*end) ERR_EXIT("bad option\n");
			id_product = strtol(argv[2] + 5, &end, 16);
			if (end != argv[2] + 9) ERR_EXIT("bad option\n");
			argc -= 2; argv += 2;
		} else if (!strcmp(argv[1], "--wait")) {
			if (argc <= 2) ERR_EXIT("bad option\n");
			wait = atoi(argv[2]) * REOPEN_FREQ;
			argc -= 2; argv += 2;
		} else if (!strcmp(argv[1], "--verbose")) {
			if (argc <= 2) ERR_EXIT("bad option\n");
			verbose = atoi(argv[2]);
			argc -= 2; argv += 2;
		} else if (argv[1][0] == '-') {
			ERR_EXIT("unknown option\n");
		} else break;
	}

	for (i = 0; ; i++) {
#if USE_LIBUSB
		device = libusb_open_device_with_vid_pid(NULL, id_vendor, id_product);
		if (device) break;
		if (i >= wait)
			ERR_EXIT("libusb_open_device failed\n");
#else
		serial = open(tty, O_RDWR | O_NOCTTY | O_SYNC);
		if (serial >= 0) break;
		if (i >= wait)
			ERR_EXIT("open(ttyUSB) failed\n");
#endif
		if (!i) DBG_LOG("Waiting for connection (%ds)\n", wait / REOPEN_FREQ);
		usleep(1000000 / REOPEN_FREQ);
	}

#if USE_LIBUSB
	io = usbio_init(device, 0);
#else
	io = usbio_init(serial, 0);
#endif
	io->verbose = verbose;

	while (argc > 1) {
		
		if (!strcmp(argv[1], "verbose")) {
			if (argc <= 2) ERR_EXIT("bad command\n");
			io->verbose = atoi(argv[2]);
			argc -= 2; argv += 2;

		} else if (!strcmp(argv[1], "serial")) {
#if USE_LIBUSB
			uint8_t serial[256];
			struct libusb_device_descriptor desc;
			libusb_device *device = libusb_get_device(io->dev_handle);
			if (!device)
				ERR_EXIT("libusb_get_device failed\n");
			if (libusb_get_device_descriptor(device, &desc) < 0)
				ERR_EXIT("libusb_get_device_descriptor failed\n");
			if (libusb_get_string_descriptor_ascii(io->dev_handle,
					desc.iSerialNumber, serial, sizeof(serial)) < 0)
				ERR_EXIT("libusb_get_string_descriptor_ascii failed\n");
			DBG_LOG("serial: \"%s\"\n", serial);
#else
			ERR_EXIT("libusb more required\n");
#endif
			argc -= 1; argv += 1;

		} else if (!strcmp(argv[1], "init")) {
			smtlink_cmd(io, 0x81, 2, 0, 0, 0);
			if (check_usbs(io, NULL)) break;
			argc -= 1; argv += 1;

		} else if (!strcmp(argv[1], "inquiry")) {
			usbc_cmd_t usbc; int len = 0x24;
			WRITE32_LE(&usbc.sig, USBC_SIG);
			WRITE32_LE(&usbc.tag, scsi_tag);
			WRITE32_LE(&usbc.data_len, len);
			usbc.flags = 0x80;
			usbc.lun = 0;
			usbc.cdb_len = 6;
			memset(usbc.cdb, 0, 16);
			usbc.cdb[0] = CMD_INQUIRY;
			WRITE16_BE(usbc.cdb + 3, len);
			usb_send(io, &usbc, USBC_LEN);

			if (usb_recv(io, len) != len) {
				DBG_LOG("unexpected response\n");
				break;
			}
			if (verbose < 2) {
				DBG_LOG("result (%d):\n", len);
				print_mem(stderr, io->buf, len);
			}
			if (check_usbs(io, NULL)) break;
			argc -= 1; argv += 1;

		} else if (!strcmp(argv[1], "flash_id")) {
			smtlink_cmd(io, 0, 4, 0, 1, 4);
			if (usb_recv(io, 4) != 4) {
				DBG_LOG("unexpected response\n");
				break;
			}
			DBG_LOG("flash_id: 0x%08x\n", READ32_LE(io->buf));
			if (check_usbs(io, NULL)) break;
			argc -= 1; argv += 1;

		} else if (!strcmp(argv[1], "reset")) {
			smtlink_cmd(io, 0x83, 0, 0, 0, 0);
			if (check_usbs(io, NULL)) break;
			argc -= 1; argv += 1;

		} else if (!strcmp(argv[1], "write_mem")) {
			const char *fn; uint64_t addr, offset, size;
			if (argc <= 5) ERR_EXIT("bad command\n");

			addr = str_to_size(argv[2]);
			offset = str_to_size(argv[3]);
			size = str_to_size(argv[4]);
			fn = argv[5];
			if ((addr | size | offset | (addr + size)) >> 32)
				ERR_EXIT("32-bit limit reached\n");
			write_mem(io, addr, offset, size, fn, blk_size);
			argc -= 5; argv += 5;

		} else if (!strcmp(argv[1], "exec")) {
			const char *fn; uint64_t addr, offset, size;
			if (argc <= 2) ERR_EXIT("bad command\n");

			addr = str_to_size(argv[2]);
			if (addr >> 32) ERR_EXIT("32-bit limit reached\n");
			smtlink_cmd(io, 10, 0, addr, 0, 0);
			if (check_usbs(io, NULL)) break;
			argc -= 2; argv += 2;

		} else if (!strcmp(argv[1], "read_flash")) {
			const char *fn; uint64_t addr, size;
			if (argc <= 4) ERR_EXIT("bad command\n");

			addr = str_to_size(argv[2]);
			size = str_to_size(argv[3]);
			if ((addr | size | (addr + size)) >> 32)
				ERR_EXIT("32-bit limit reached\n");
			fn = argv[4];
			dump_flash(io, addr, size, fn, blk_size);
			argc -= 4; argv += 4;

		} else if (!strcmp(argv[1], "check_flash")) {
			const char *fn; uint64_t addr, size;
			if (argc <= 3) ERR_EXIT("bad command\n");

			addr = str_to_size(argv[2]);
			size = str_to_size(argv[3]);
			if ((addr | size | (addr + size)) >> 32)
				ERR_EXIT("32-bit limit reached\n");

			smtlink_cmd(io, 0x82, size, addr, 1, 2);
			if (usb_recv(io, 2) != 2) {
				DBG_LOG("unexpected response\n");
				break;
			}
			DBG_LOG("flash_crc: 0x%04x\n", READ16_LE(io->buf));
			if (check_usbs(io, NULL)) break;
			argc -= 3; argv += 3;

		} else if (!strcmp(argv[1], "blk_size")) {
			if (argc <= 2) ERR_EXIT("bad command\n");
			blk_size = str_to_size(argv[2]);
			blk_size = blk_size < 0 ? 1 :
					blk_size > 0x1000 ? 0x1000 : blk_size;
			argc -= 2; argv += 2;

		} else if (!strcmp(argv[1], "flash_idx")) {
			if (argc <= 2) ERR_EXIT("bad command\n");
			flash_idx = strtol(argv[2], NULL, 0);
			flash_idx &= 15;
			argc -= 2; argv += 2;

		} else if (!strcmp(argv[1], "timeout")) {
			if (argc <= 2) ERR_EXIT("bad command\n");
			io->timeout = atoi(argv[2]);
			argc -= 2; argv += 2;

		} else {
			ERR_EXIT("unknown command\n");
		}
	}

	usbio_free(io);
#if USE_LIBUSB
	libusb_exit(NULL);
#endif
	return 0;
}
