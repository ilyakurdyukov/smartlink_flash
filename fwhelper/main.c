#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

static int crc16(const uint8_t *s, unsigned n) {
	unsigned t, c = 0xffff;
	while (n--)
#if 0
	for (c ^= *s++ << 8, t = 8; t--;)
		c <<= 1, c ^= (0 - (c >> 16)) & 0x11021;
#else // fast
		t = c >> 8 ^ *s++, t ^= t >> 4,
		c = (c << 8 ^ t ^ t << 5 ^ t << 12) & 0xffff;
#endif
	return c;
}

#define ERR_EXIT(...) \
	do { fprintf(stderr, __VA_ARGS__); return 1; } while (0)

#define READ32_LE(p) ( \
	((uint8_t*)(p))[3] << 24 | \
	((uint8_t*)(p))[2] << 16 | \
	((uint8_t*)(p))[1] << 8 | \
	((uint8_t*)(p))[0])

#define WRITE16_LE(p, a) do { \
  uint32_t __tmp = a; \
	((uint8_t*)(p))[0] = (uint8_t)__tmp; \
	((uint8_t*)(p))[1] = (uint8_t)(__tmp >> 8); \
} while (0)

#define WRITE32_LE(p, a) do { \
  uint32_t __tmp = a; \
	((uint8_t*)(p))[0] = (uint8_t)__tmp; \
	((uint8_t*)(p))[1] = (uint8_t)(__tmp >> 8); \
	((uint8_t*)(p))[2] = (uint8_t)(__tmp >> 16); \
	((uint8_t*)(p))[3] = (uint8_t)(__tmp >> 24); \
} while (0)

static int dump2fw(uint8_t *mem, unsigned size, const char *fn) {
	FILE *fo;
	uint32_t fw_size = 0;

	if (size < 0x60 || memcmp(mem, "HLKJ", 4))
		ERR_EXIT("bad bootloader header\n");

	{
		uint32_t i, n, len, off = READ32_LE(mem + 0x20);
		uint8_t *p = mem + off;
		if (size < off || size - off < 0x100)
			ERR_EXIT("data outside the file\n");

		n = READ32_LE(p);
		if (n > 15) ERR_EXIT("too many partitions\n");
		for (i = 0; i < n; i++) {
			p += 16;
			if (!memcmp(p, "PSMP", 4)) continue;
			off = READ32_LE(p + 4);
			len = READ32_LE(p + 8);
			if (size < off || size - off < len)
				ERR_EXIT("data outside the file\n");
			if (crc16(mem + off, len) != READ32_LE(p + 12))
				ERR_EXIT("partition checksum mismatch\n");
			len += off;
			if (fw_size < len) fw_size = len;
		}
	}
	if (!fw_size)
		ERR_EXIT("no firmware partitions found\n");

	if (!(fo = fopen(fn, "wb")))
		ERR_EXIT("fopen(output) failed\n");

	{
		uint8_t buf[0x100];
		memset(buf, 0, 0x100);
		memcpy(buf, "CONFIG", 6);
		WRITE16_LE(buf + 6, 0x100);
		WRITE32_LE(buf + 0x10, fw_size);
		WRITE16_LE(buf + 0x14, crc16(mem, fw_size));
		memcpy(buf + 0x16, "SL6801", 6);
		buf[0xfe] = 0x55;
		buf[0xff] = 0xaa;
		fwrite(buf, 1, 0x100, fo);
	}
	fwrite(mem, 1, fw_size, fo);
	fclose(fo);
	return 0;
}

int main(int argc, char **argv) {
	uint8_t *mem; size_t size = 0;

	if (argc < 3)
		ERR_EXIT("Usage: %s flash.bin cmd args...\n", argv[0]);

	mem = loadfile(argv[1], &size);
	if (!mem) ERR_EXIT("loadfile failed\n");
	argc -= 1; argv += 1;

	/* Detect dump larger than flash size. */
	{
		unsigned k = 1 << 20;
		for (; !(k >> 24) && size > k; k <<= 1)
			if (!memcmp(mem, mem + k, size - k)) {
				size = k;
				break;
			}
	}

	while (argc > 1) {
		if (!strcmp(argv[1], "dump2fw")) {
			const char *fn;
			if (argc <= 2) ERR_EXIT("bad command\n");
			fn = argv[2];
			dump2fw(mem, size, fn);
			argc -= 2; argv += 2;
		} else {
			ERR_EXIT("unknown command\n");
		}
	}
}

