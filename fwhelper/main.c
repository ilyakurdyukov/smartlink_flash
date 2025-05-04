#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

#define READ16_LE(p) ( \
	((uint8_t*)(p))[1] << 8 | \
	((uint8_t*)(p))[0])

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

static void id2str(char *buf, uint8_t *s) {
	int i;
	for (i = 0; i < 4; i++) {
		unsigned a = s[i];
		if (a - 0x20 < 0x5f) *buf++ = a;
		else sprintf(buf, "\\x%02x", a), buf += 4;
	}
	*buf = 0;
}

static int dump2fw(uint8_t *mem, unsigned size, const char *fn) {
	FILE *fo;
	uint32_t fw_size = 0;
	const char *chip_name = "SL6801";

	if (size < 0x60 || memcmp(mem, "HLKJ", 4))
		ERR_EXIT("bad bootloader header\n");

	{
		uint32_t i, n, len, off = READ32_LE(mem + 0x20), ver;
		uint8_t *p = mem + off;
		if (size < off || size - off < 0x100)
			ERR_EXIT("data outside the file\n");

		n = READ32_LE(p); ver = READ32_LE(p + 8);
		if (n > 15) ERR_EXIT("too many partitions\n");
		for (i = 0; i < n; i++) {
			p += 16;
			if (!memcmp(p, "PSMP", 4)) continue;
			off = READ32_LE(p + 4);
			len = READ32_LE(p + 8);
			if (size < off || size - off < len)
				ERR_EXIT("data outside the file\n");
			{
				int chk1, chk2;
				chk1 = crc16(mem + off, len);
				chk2 = READ16_LE(p + (ver >= 0x30 ? 14 : 12));
				if (chk1 != chk2) {
					char name[4 * 4 + 1];
					id2str(name, p);
					ERR_EXIT("partition \"%s\" checksum mismatch (0x%04x, expected 0x%04x)\n", name, chk1, chk2);
				}
			}
			len += off;
			if (fw_size < len) fw_size = len;
		}
		/* heuristic */
		if (ver >= 0x30) chip_name = "SL6806";
	}
	if (!fw_size)
		ERR_EXIT("no firmware partitions found\n");

	if (!(fo = fopen(fn, "wb")))
		ERR_EXIT("fopen(output) failed\n");

	{
		uint8_t buf[0x100];
		memset(buf, 0, 0x100);
		memcpy(buf, "CONFIG", 6);
		WRITE32_LE(buf + 6, 0x100);
		WRITE32_LE(buf + 0x10, fw_size);
		WRITE16_LE(buf + 0x14, crc16(mem, fw_size));
		strcpy((char*)buf + 0x16, chip_name);
		buf[0xfe] = 0x55;
		buf[0xff] = 0xaa;
		fwrite(buf, 1, 0x100, fo);
	}
	fwrite(mem, 1, fw_size, fo);
	fclose(fo);
	return 0;
}

static int scan_firm(uint8_t *mem0, uint8_t *mem, unsigned size, int flags) {
	uint32_t off, moff = mem - mem0;
	if (size < 0x30)
		ERR_EXIT("too short FIRM partition\n");
	off = READ32_LE(mem);
	printf("0x%x: FIRM, timestamp = %u\n",
			moff, READ32_LE(mem + 4));
	return 0;
}

static int scan_fw(uint8_t *mem, unsigned size, int flags) {
	uint32_t moff = 0;
	uint8_t *mem0 = mem;

	if (size >= 0x100 && !memcmp(mem, "CONFIG", 6)) {
		moff = READ32_LE(mem + 6);
		if (moff != 0x100)
			ERR_EXIT("unexpected CONFIG size\n");
		if (size < moff)
			ERR_EXIT("data outside the file\n");
		printf("0x%x: CONFIG, chip = ", 0);
		print_string(stdout, mem + 0x16, mem[0x16 + 9] ? 10 : strlen((char*)mem + 0x16));
		size -= moff;
		{
			uint32_t len = READ32_LE(mem + 0x10);
			int chk1, chk2;
			if (size < len)
				ERR_EXIT("data outside the file\n");
			size = len;
			chk1 = crc16(mem + moff, len);
			chk2 = READ16_LE(mem + 0x14);
			if (chk1 != chk2)
				printf("CONFIG checksum mismatch (0x%04x, expected 0x%04x)\n", chk1, chk2);
		}
		mem += moff;
	}

	if (size < 0x60 || memcmp(mem, "HLKJ", 4))
		ERR_EXIT("bad bootloader header\n");
	printf("0x%x: HLKJ, base = 0x%x, entry = 0x%x",
			(int)(mem - mem0), READ32_LE(mem + 4), READ32_LE(mem + 8));
	{
		uint32_t len, off = READ32_LE(mem + 12);
		len = READ32_LE(mem + 0x10);
		printf(", off = 0x%x, size = 0x%x\n", off + moff, len);
		if (size < off || size - off < len)
			printf("data outside the file\n");
		else {
			int chk1, chk2;
			chk1 = crc16(mem + off, len);
			chk2 = READ32_LE(mem + 0x14);
			if (chk1 != chk2)
				printf("bootloader checksum mismatch (0x%04x, expected 0x%04x)\n", chk1, chk2);
		}
	}

	{
		uint32_t i, n, len, off = READ32_LE(mem + 0x20), ver;
		uint8_t *p = mem + off;
		if (size < off || size - off < 0x100)
			ERR_EXIT("data outside the file\n");
		n = READ32_LE(p); ver = READ32_LE(p + 8);
		printf("0x%x: partition table (items = %u, ver = 0x%x)\n", (int)(p - mem0), n, ver);
		if (n > 15) ERR_EXIT("too many partitions\n");
		for (i = 0; i < n; i++) {
			char name[4 * 4 + 1];
			int psmp;
			p += 16;
			id2str(name, p);
			off = READ32_LE(p + 4);
			len = READ32_LE(p + 8);
			printf("partition \"%s\", off = 0x%x, size = 0x%x\n", name, off + moff, len);

			psmp = !memcmp(p, "PSMP", 4);
			if (size < off || size - off < len) {
				if (!psmp) printf("data outside the file\n");
				continue;
			}
			if (!psmp) {
				int chk1, chk2;
				chk1 = crc16(mem + off, len);
				chk2 = READ16_LE(p + (ver >= 0x30 ? 14 : 12));
				if (chk1 != chk2)
					printf("partition checksum mismatch (0x%04x, expected 0x%04x)\n", chk1, chk2);
			}
			if (!memcmp(p, "FIRM", 4))
				scan_firm(mem0, mem + off, len, flags);
		}
	}
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
		} else if (!strcmp(argv[1], "scan")) {
			scan_fw(mem, size, 0);
			argc -= 1; argv += 1;
		} else {
			ERR_EXIT("unknown command\n");
		}
	}
}

