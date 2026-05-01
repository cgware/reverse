#include "arch.h"
#include "elfc_internal.h"
#include "format.h"

#include "bin.h"
#include "mem.h"
#include "t_drivers.h"
#include "test.h"

#include <string.h>

static void t_format_elf_put8(byte *buf, size_t off, byte v)
{
	buf[off] = v;
}

static void t_format_elf_put16(byte *buf, size_t off, u16 v)
{
	buf[off + 0] = (byte)(v >> 0);
	buf[off + 1] = (byte)(v >> 8);
}

static void t_format_elf_put32(byte *buf, size_t off, u32 v)
{
	buf[off + 0] = (byte)(v >> 0);
	buf[off + 1] = (byte)(v >> 8);
	buf[off + 2] = (byte)(v >> 16);
	buf[off + 3] = (byte)(v >> 24);
}

static void t_format_elf_put64(byte *buf, size_t off, u64 v)
{
	t_format_elf_put32(buf, off + 0, (u32)(v >> 0));
	t_format_elf_put32(buf, off + 4, (u32)(v >> 32));
}

static size_t t_format_elf_build(byte *buf, size_t cap)
{
	mem_set(buf, 0, cap);

	const size_t phoff	= 0x100;
	const size_t shoff	= 0x200;
	const size_t interp_off = 0x500;
	const size_t shstr_off	= 0x900;

	size_t off = 0;
	t_format_elf_put32(buf, off, 0x464C457F);
	off += 4;
	t_format_elf_put8(buf, off++, 2);
	t_format_elf_put8(buf, off++, 1);
	t_format_elf_put8(buf, off++, 1);
	t_format_elf_put8(buf, off++, 0);
	t_format_elf_put8(buf, off++, 0);
	mem_set(buf + off, 0, 7);
	off += 7;

	t_format_elf_put16(buf, off, 2);
	off += 2;
	t_format_elf_put16(buf, off, 0x3E);
	off += 2;
	t_format_elf_put32(buf, off, 1);
	off += 4;
	t_format_elf_put64(buf, off, 0);
	off += 8;
	t_format_elf_put64(buf, off, phoff);
	off += 8;
	t_format_elf_put64(buf, off, shoff);
	off += 8;
	t_format_elf_put32(buf, off, 0);
	off += 4;
	t_format_elf_put16(buf, off, 64);
	off += 2;
	t_format_elf_put16(buf, off, 56);
	off += 2;
	t_format_elf_put16(buf, off, 1);
	off += 2;
	t_format_elf_put16(buf, off, 64);
	off += 2;
	t_format_elf_put16(buf, off, 3);
	off += 2;
	t_format_elf_put16(buf, off, 2);
	off += 2;

	t_format_elf_put32(buf, phoff + 0, 1);
	t_format_elf_put32(buf, phoff + 4, 5);
	t_format_elf_put64(buf, phoff + 8, interp_off);
	t_format_elf_put64(buf, phoff + 16, 0x400000);
	t_format_elf_put64(buf, phoff + 24, 0x400000);
	t_format_elf_put64(buf, phoff + 32, 0x120);
	t_format_elf_put64(buf, phoff + 40, 0x120);
	t_format_elf_put64(buf, phoff + 48, 0x1000);

	const strv_t names[] = {
		STRVT(""),
		STRVT(".interp"),
		STRVT(".shstrtab"),
	};

	size_t name_offs[sizeof(names) / sizeof(names[0])] = {0};
	size_t name_len					   = 0;
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		name_offs[i] = name_len;
		size_t n     = names[i].len + 1;
		mem_copy(buf + shstr_off + name_len, cap - (shstr_off + name_len), names[i].data, n);
		name_len += n;
	}

	mem_copy(buf + interp_off, 10, "/bin/true", 10);

	struct {
		size_t name;
		u32 type;
		u64 flags;
		u64 addr;
		u64 off;
		u64 size;
		u32 link;
		u32 info;
		u64 align;
		u64 entsize;
	} sects[] = {
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{name_offs[1], 1, 0, 0, interp_off, 10, 0, 0, 1, 0},
		{name_offs[2], 3, 0, 0, shstr_off, name_len, 0, 0, 1, 0},
	};

	for (size_t i = 0; i < sizeof(sects) / sizeof(sects[0]); i++) {
		size_t base = shoff + i * 64;
		t_format_elf_put32(buf, base + 0, (u32)sects[i].name);
		t_format_elf_put32(buf, base + 4, sects[i].type);
		t_format_elf_put64(buf, base + 8, sects[i].flags);
		t_format_elf_put64(buf, base + 16, sects[i].addr);
		t_format_elf_put64(buf, base + 24, sects[i].off);
		t_format_elf_put64(buf, base + 32, sects[i].size);
		t_format_elf_put32(buf, base + 40, sects[i].link);
		t_format_elf_put32(buf, base + 44, sects[i].info);
		t_format_elf_put64(buf, base + 48, sects[i].align);
		t_format_elf_put64(buf, base + 56, sects[i].entsize);
	}

	return shstr_off + name_len;
}

static int t_format_elf_parse_bin(const bin_t *bin, asmc_t *asmc, alloc_t alloc)
{
	format_driver_t *format = format_driver_find(STRV("elf"));
	arch_driver_t *arch	= arch_driver_find(STRV("x86"));
	if (format == NULL || arch == NULL) {
		return 1;
	}

	reverse_image_t image = {0};
	if (reverse_image_init(&image, alloc) == NULL) {
		return 1;
	}

	int ret = format->parse(format, bin, &image, DST_NONE(), alloc);
	if (ret == 0) {
		ret = arch->parse(arch, &image, alloc);
	}
	if (ret == 0) {
		ret = format->emit(format, &image, asmc, alloc);
	}

	if (format->free != NULL) {
		format->free(format, &image);
	}
	reverse_image_free(&image);
	return ret;
}

TEST(format_elf_read_magic)
{
	START;

	format_driver_t *drv = format_driver_find(STRV("elf"));
	EXPECT_NE(drv, NULL);

	const u8 bad[] = {0x00, 0x01, 0x02, 0x03};
	bin_t bin      = {0};
	bin_init(&bin, sizeof(bad), ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, bad, sizeof(bad)), 0);

	asmc_t asmc = {0};
	asmc_init(&asmc, 2, ALLOC_STD);
	EXPECT_NE(t_format_elf_parse_bin(&bin, &asmc, ALLOC_STD), 0);

	bin_free(&bin);
	asmc_free(&asmc);

	END;
}

TEST(format_elf_self)
{
	START;

	byte image[0xA00] = {0};
	size_t len	  = t_format_elf_build(image, sizeof(image));
	EXPECT_GT(len, 0);

	bin_t bin = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, len), 0);

	asmc_t asmc = {0};
	asmc_init(&asmc, 64, ALLOC_STD);
	EXPECT_EQ(t_format_elf_parse_bin(&bin, &asmc, ALLOC_STD), 0);
	EXPECT_GT(asmc.ops.cnt, 0);

	bin_free(&bin);
	asmc_free(&asmc);

	END;
}

TEST(format_elf_report_dst)
{
	START;

	byte image[0xA00] = {0};
	size_t len	  = t_format_elf_build(image, sizeof(image));
	EXPECT_GT(len, 0);

	bin_t bin = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, len), 0);

	format_driver_t *format = format_driver_find(STRV("elf"));
	EXPECT_NE(format, NULL);

	reverse_image_t parsed = {0};
	EXPECT_NE(reverse_image_init(&parsed, ALLOC_STD), NULL);

	char report[4096] = {0};
	if (format != NULL) {
		EXPECT_EQ(format->parse(format, &bin, &parsed, DST_BUF(report), ALLOC_STD), 0);
		EXPECT_NE(strstr(report, "[ELF header]"), NULL);
		EXPECT_NE(strstr(report, "[Section header]"), NULL);
		EXPECT_EQ(parsed.sections.cnt, 3);
		if (format->free != NULL) {
			format->free(format, &parsed);
		}
	}

	reverse_image_free(&parsed);
	bin_free(&bin);

	END;
}

TEST(format_elf_error_paths)
{
	START;

	format_driver_t *format = format_driver_find(STRV("elf"));
	EXPECT_NE(format, NULL);

	reverse_image_t image = {0};
	EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);
	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 4, ALLOC_STD), &asmc);

	if (format != NULL) {
		EXPECT_EQ(format->emit(format, NULL, &asmc, ALLOC_STD), 1);
		EXPECT_EQ(format->emit(format, &image, NULL, ALLOC_STD), 1);
		EXPECT_EQ(format->emit(format, &image, &asmc, ALLOC_STD), 1);

		image.priv = &image;
		reverse_image_section_t exec = {
			.name  = STRVT(".text"),
			.flags = REVERSE_IMAGE_SECTION_EXEC,
		};
		EXPECT_NE(reverse_image_add_section(&image, &exec, NULL), NULL);
		EXPECT_EQ(format->emit(format, &image, &asmc, ALLOC_STD), 1);
		image.priv = NULL;

		byte elf_bytes[0xA00] = {0};
		size_t len		= t_format_elf_build(elf_bytes, sizeof(elf_bytes));
		bin_t bin		= {0};
		EXPECT_NE(bin_init(&bin, len, ALLOC_STD), NULL);
		EXPECT_EQ(t_drivers_bin_from_bytes(&bin, elf_bytes, len), 0);

		reverse_image_t parsed = {0};
		EXPECT_NE(reverse_image_init(&parsed, ALLOC_STD), NULL);
		EXPECT_EQ(reverse_image_set_bin(&parsed, &bin), 0);
		mem_oom(1);
		EXPECT_EQ(format->parse(format, &bin, &parsed, DST_NONE(), ALLOC_STD), 1);
		mem_oom(0);

		reverse_image_free(&parsed);
		bin_free(&bin);
	}

	EXPECT_EQ(elfc_parse(NULL, &image, DST_NONE(), ALLOC_STD), 1);
	EXPECT_EQ(elfc_parse(&(elfc_t){0}, NULL, DST_NONE(), ALLOC_STD), 1);

	elfc_t elfc = {0};
	EXPECT_EQ(elfc_init(&elfc, ALLOC_STD), &elfc);
	elfc_sect_t *sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_ELF_IDENT;
		size_t off = 0;
		EXPECT_EQ(elfc_parse_ident(&elfc, &off, sect, ALLOC_STD), 0);
	}
	elfc_free(&elfc);

	asmc_free(&asmc);
	reverse_image_free(&image);

	END;
}

STEST(format_elf)
{
	SSTART;

	RUN(format_elf_read_magic);
	RUN(format_elf_self);
	RUN(format_elf_report_dst);
	RUN(format_elf_error_paths);

	SEND;
}
