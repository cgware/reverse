#include "arch.h"
#include "elfc.h"
#include "elfc_internal.h"
#include "format.h"
#include "parse_x86_internal.h"

#include "bin.h"
#include "log.h"
#include "mem.h"
#include "t_drivers.h"
#include "test.h"

static void t_x86_put8(byte *buf, size_t off, byte v)
{
	buf[off] = v;
}

static void t_x86_put16(byte *buf, size_t off, u16 v)
{
	buf[off + 0] = (byte)(v >> 0);
	buf[off + 1] = (byte)(v >> 8);
}

static void t_x86_put32(byte *buf, size_t off, u32 v)
{
	buf[off + 0] = (byte)(v >> 0);
	buf[off + 1] = (byte)(v >> 8);
	buf[off + 2] = (byte)(v >> 16);
	buf[off + 3] = (byte)(v >> 24);
}

static void t_x86_put64(byte *buf, size_t off, u64 v)
{
	t_x86_put32(buf, off + 0, (u32)(v >> 0));
	t_x86_put32(buf, off + 4, (u32)(v >> 32));
}

static size_t t_x86_append_cstr(byte *buf, size_t off, strv_t str)
{
	mem_copy(buf + off, str.len + 1, str.data, str.len);
	buf[off + str.len] = '\0';
	return off + str.len + 1;
}

static size_t t_x86_build_elf_full(byte *buf, size_t cap)
{
	mem_set(buf, 0, cap);

	const size_t phoff	 = 0x100;
	const size_t shoff	 = 0x200;
	const size_t interp_off	 = 0x500;
	const size_t note_off	 = 0x540;
	const size_t strtab_off	 = 0x640;
	const size_t dynstr_off	 = 0x680;
	const size_t dynamic_off = 0x700;
	const size_t dynsym_off	 = 0x760;
	const size_t reladyn_off = 0x7C0;
	const size_t text_off	 = 0x840;
	const size_t bytes_off	 = 0x900;
	const size_t shstr_off	 = 0xA00;

	size_t off = 0;
	t_x86_put32(buf, off, 0x464C457F);
	off += 4;
	t_x86_put8(buf, off++, 2);
	t_x86_put8(buf, off++, 1);
	t_x86_put8(buf, off++, 1);
	t_x86_put8(buf, off++, 0);
	t_x86_put8(buf, off++, 0);
	mem_set(buf + off, 0, 7);
	off += 7;

	t_x86_put16(buf, off, 2);
	off += 2;
	t_x86_put16(buf, off, 0x3E);
	off += 2;
	t_x86_put32(buf, off, 1);
	off += 4;
	t_x86_put64(buf, off, 0x400000);
	off += 8;
	t_x86_put64(buf, off, phoff);
	off += 8;
	t_x86_put64(buf, off, shoff);
	off += 8;
	t_x86_put32(buf, off, 0);
	off += 4;
	t_x86_put16(buf, off, 64);
	off += 2;
	t_x86_put16(buf, off, 56);
	off += 2;
	t_x86_put16(buf, off, 2);
	off += 2;
	t_x86_put16(buf, off, 64);
	off += 2;
	t_x86_put16(buf, off, 11);
	off += 2;
	t_x86_put16(buf, off, 10);
	off += 2;

	// Program headers.
	t_x86_put32(buf, phoff + 0, 1);
	t_x86_put32(buf, phoff + 4, 5);
	t_x86_put64(buf, phoff + 8, interp_off);
	t_x86_put64(buf, phoff + 16, 0x400000);
	t_x86_put64(buf, phoff + 24, 0x400000);
	t_x86_put64(buf, phoff + 32, 0x20);
	t_x86_put64(buf, phoff + 40, 0x20);
	t_x86_put64(buf, phoff + 48, 1);

	t_x86_put32(buf, phoff + 56, 1);
	t_x86_put32(buf, phoff + 60, 4);
	t_x86_put64(buf, phoff + 64, text_off);
	t_x86_put64(buf, phoff + 72, 0x400040);
	t_x86_put64(buf, phoff + 80, 0x400040);
	t_x86_put64(buf, phoff + 88, 0x40);
	t_x86_put64(buf, phoff + 96, 0x40);
	t_x86_put64(buf, phoff + 104, 0x1000);

	const strv_t sh_names[] = {
		STRVT(""),
		STRVT(".interp"),
		STRVT(".note"),
		STRVT(".strtab"),
		STRVT(".dynstr"),
		STRVT(".dynamic"),
		STRVT(".dynsym"),
		STRVT(".rela.dyn"),
		STRVT(".text"),
		STRVT(".bytes"),
		STRVT(".shstrtab"),
	};

	size_t sh_name_offs[sizeof(sh_names) / sizeof(sh_names[0])] = {0};
	size_t sh_name_len					    = 0;
	for (size_t i = 0; i < sizeof(sh_names) / sizeof(sh_names[0]); i++) {
		sh_name_offs[i] = sh_name_len;
		sh_name_len	= t_x86_append_cstr(buf, shstr_off + sh_name_len, sh_names[i]) - shstr_off;
	}

	const strv_t dyn_names[] = {
		STRVT(""),
		STRVT("libc.so.6"),
		STRVT("puts"),
	};

	size_t dyn_name_offs[sizeof(dyn_names) / sizeof(dyn_names[0])] = {0};
	size_t dyn_name_len					       = 0;
	for (size_t i = 0; i < sizeof(dyn_names) / sizeof(dyn_names[0]); i++) {
		dyn_name_offs[i] = dyn_name_len;
		dyn_name_len	 = t_x86_append_cstr(buf, dynstr_off + dyn_name_len, dyn_names[i]) - dynstr_off;
	}

	mem_copy(buf + interp_off, 10, "/bin/true", 10);

	size_t str_name_len	 = 0;
	const strv_t str_names[] = {
		STRVT(""),
		STRVT("alpha"),
		STRVT("beta"),
	};
	for (size_t i = 0; i < sizeof(str_names) / sizeof(str_names[0]); i++) {
		str_name_len = t_x86_append_cstr(buf, strtab_off + str_name_len, str_names[i]) - strtab_off;
	}

	// Note section: ABI tag, build ids, GNU properties, and unknown note.
	size_t note = note_off;
	t_x86_put32(buf, note + 0, 4);
	t_x86_put32(buf, note + 4, 16);
	t_x86_put32(buf, note + 8, 1);
	mem_copy(buf + note + 12, 4, "GNU", 4);
	t_x86_put32(buf, note + 16, 0);
	t_x86_put32(buf, note + 20, 1);
	t_x86_put32(buf, note + 24, 2);
	t_x86_put32(buf, note + 28, 3);
	note += 32;

	t_x86_put32(buf, note + 0, 4);
	t_x86_put32(buf, note + 4, 20);
	t_x86_put32(buf, note + 8, 3);
	mem_copy(buf + note + 12, 4, "GNU", 4);
	for (size_t i = 0; i < 20; i++) {
		buf[note + 16 + i] = (byte)(i + 1);
	}
	note += 36;

	t_x86_put32(buf, note + 0, 4);
	t_x86_put32(buf, note + 4, 16);
	t_x86_put32(buf, note + 8, 3);
	mem_copy(buf + note + 12, 4, "GNU", 4);
	for (size_t i = 0; i < 16; i++) {
		buf[note + 16 + i] = (byte)(0x80 + i);
	}
	note += 32;

	t_x86_put32(buf, note + 0, 4);
	t_x86_put32(buf, note + 4, 40);
	t_x86_put32(buf, note + 8, 5);
	mem_copy(buf + note + 12, 4, "GNU", 4);
	t_x86_put32(buf, note + 16, 0xc0000002);
	t_x86_put32(buf, note + 20, 4);
	t_x86_put32(buf, note + 24, 3);
	t_x86_put32(buf, note + 28, 0);
	t_x86_put32(buf, note + 32, 0xC0008002);
	t_x86_put32(buf, note + 36, 4);
	t_x86_put32(buf, note + 40, 1);
	t_x86_put32(buf, note + 44, 0);
	t_x86_put32(buf, note + 48, 0xDEADBEEF);
	t_x86_put32(buf, note + 52, 0);
	note += 56;

	t_x86_put32(buf, note + 0, 4);
	t_x86_put32(buf, note + 4, 4);
	t_x86_put32(buf, note + 8, 0x1234);
	mem_copy(buf + note + 12, 4, "GNU", 4);
	t_x86_put32(buf, note + 16, 0xDEADBEEF);
	note += 20;

	// Dynamic and relocation tables.
	t_x86_put64(buf, dynamic_off + 0, 0x1);
	t_x86_put64(buf, dynamic_off + 8, dyn_name_offs[1]);
	t_x86_put64(buf, dynamic_off + 16, 0x5);
	t_x86_put64(buf, dynamic_off + 24, dynstr_off);
	t_x86_put64(buf, dynamic_off + 32, 0);
	t_x86_put64(buf, dynamic_off + 40, 0);

	t_x86_put32(buf, dynsym_off + 0, dyn_name_offs[0]);
	t_x86_put8(buf, dynsym_off + 4, 0);
	t_x86_put8(buf, dynsym_off + 5, 0);
	t_x86_put16(buf, dynsym_off + 6, 0);
	t_x86_put64(buf, dynsym_off + 8, 0);
	t_x86_put64(buf, dynsym_off + 16, 0);

	t_x86_put32(buf, dynsym_off + 24, dyn_name_offs[2]);
	t_x86_put8(buf, dynsym_off + 28, 0x12);
	t_x86_put8(buf, dynsym_off + 29, 0);
	t_x86_put16(buf, dynsym_off + 30, 0);
	t_x86_put64(buf, dynsym_off + 32, 0x401000);
	t_x86_put64(buf, dynsym_off + 40, 4);

	t_x86_put64(buf, reladyn_off + 0, 0x401200);
	t_x86_put32(buf, reladyn_off + 8, 6);
	t_x86_put32(buf, reladyn_off + 12, 1);
	t_x86_put64(buf, reladyn_off + 16, 0);

	// Program bytes.
	size_t text	  = text_off;
	byte text_bytes[] = {
		0x90,
		0x31,
		0xC0,
		0xF3,
		0x0F,
		0x05,
		0xF3,
		0x0F,
		0x1E,
		0xFA,
		0xF3,
		0x0F,
		0x1F,
		0x00,
		0xC3,
	};
	mem_copy(buf + text, sizeof(text_bytes), text_bytes, sizeof(text_bytes));

	for (size_t i = 0; i < 32; i++) {
		buf[bytes_off + i] = (byte)(0xA0 + i);
	}

	// Section headers.
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
		{sh_name_offs[1], 1, 0, 0, interp_off, 0x20, 0, 0, 1, 0},
		{sh_name_offs[2], 7, 0, 0, note_off, note - note_off, 0, 0, 4, 0},
		{sh_name_offs[3], 3, 0, 0, strtab_off, str_name_len, 0, 0, 1, 0},
		{sh_name_offs[4], 3, 0, 0, dynstr_off, dyn_name_len, 0, 0, 1, 0},
		{sh_name_offs[5], 6, 0, 0, dynamic_off, 48, 4, 0, 8, 16},
		{sh_name_offs[6], 11, 0, 0, dynsym_off, 48, 4, 0, 8, 24},
		{sh_name_offs[7], 4, 0, 0, reladyn_off, 24, 6, 6, 8, 24},
		{sh_name_offs[8], 1, 1 << 2, 0, text_off, sizeof(text_bytes), 0, 0, 16, 0},
		{sh_name_offs[9], 1, (1 << 0) | (1 << 1), 0, bytes_off, 32, 0, 0, 1, 0},
		{sh_name_offs[10], 3, 0, 0, shstr_off, sh_name_len, 0, 0, 1, 0},
	};

	for (size_t i = 0; i < sizeof(sects) / sizeof(sects[0]); i++) {
		size_t base = shoff + i * 64;
		t_x86_put32(buf, base + 0, (u32)sects[i].name);
		t_x86_put32(buf, base + 4, sects[i].type);
		t_x86_put64(buf, base + 8, sects[i].flags);
		t_x86_put64(buf, base + 16, sects[i].addr);
		t_x86_put64(buf, base + 24, sects[i].off);
		t_x86_put64(buf, base + 32, sects[i].size);
		t_x86_put32(buf, base + 40, sects[i].link);
		t_x86_put32(buf, base + 44, sects[i].info);
		t_x86_put64(buf, base + 48, sects[i].align);
		t_x86_put64(buf, base + 56, sects[i].entsize);
	}

	return shstr_off + sh_name_len;
}

static size_t t_x86_build_program_blob(byte *buf, size_t cap)
{
	mem_set(buf, 0, cap);

	const byte bytes[] = {
		0x01, 0xDB, 0x29, 0xDB, 0x39, 0xDB, 0x85, 0xDB, 0x31, 0xDB, 0x48, 0x31, 0xC0, 0x80, 0x3D, 0x10, 0x00, 0x00, 0x00,
		0x7F, 0x83, 0xC0, 0x01, 0x83, 0xE4, 0x02, 0x83, 0xE8, 0x03, 0x89, 0xD8, 0x8B, 0x05, 0x04, 0x00, 0x00, 0x00, 0x8D,
		0x05, 0x08, 0x00, 0x00, 0x00, 0x90, 0xB8, 0x78, 0x56, 0x34, 0x12, 0xC1, 0xE8, 0x01, 0xC1, 0xF8, 0x02, 0xC6, 0x05,
		0x10, 0x00, 0x00, 0x00, 0xAA, 0xC7, 0xC0, 0x44, 0x33, 0x22, 0x11, 0xD1, 0xF8, 0xE8, 0x20, 0x00, 0x00, 0x00, 0xE9,
		0x30, 0x00, 0x00, 0x00, 0xF4, 0x74, 0x05, 0x75, 0x06, 0xF2, 0xFF, 0x25, 0x40, 0x00, 0x00, 0x00, 0xFF, 0x25, 0x44,
		0x00, 0x00, 0x00, 0xF3, 0xFF, 0x15, 0x48, 0x00, 0x00, 0x00, 0xFF, 0xD0, 0xFF, 0xE0, 0xF3, 0x0F, 0x05, 0xF3, 0x0F,
		0x1E, 0xFA, 0x66, 0x0F, 0x1F, 0x44, 0x24, 0x7F, 0x2E, 0x0F, 0x1F, 0x84, 0x24, 0x01, 0x02, 0x03, 0x04, 0x00,
	};

	mem_copy(buf, sizeof(bytes), bytes, sizeof(bytes));
	return sizeof(bytes);
}

static int t_x86_set_elfc_bytes(elfc_t *elfc, const byte *data, size_t len)
{
	if (buf_resize(&elfc->bytes, len) != 0) {
		return 1;
	}

	elfc->bytes.used = len;
	mem_copy(elfc->bytes.data, len, data, len);
	return 0;
}

static int t_x86_parse_elf_bin(const bin_t *bin, asmc_t *asmc, alloc_t alloc)
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

static int t_x86_parse_program_blob(const byte *data, size_t len, u8 mode, size_t *ops_cnt)
{
	bin_t bin = {0};
	bin_init(&bin, len, ALLOC_STD);
	int ret = t_drivers_bin_from_bytes(&bin, data, len);

	asmc_t asmc = {0};
	if (ret == 0) {
		ret = x86_parse_program_section(&bin, 0, len, mode, &asmc, ALLOC_STD);
	}
	if (ret == 0 && ops_cnt != NULL) {
		*ops_cnt = asmc.ops.cnt;
	}

	asmc_free(&asmc);
	bin_free(&bin);
	return ret;
}

static int t_x86_parse_program_blob_add_op_failure(const byte *data, size_t len, u8 mode)
{
	bin_t bin = {0};
	bin_init(&bin, len, ALLOC_STD);
	int ret = t_drivers_bin_from_bytes(&bin, data, len);

	asmc_t asmc = {0};
	if (ret == 0 && asmc_init(&asmc, 1, ALLOC_STD) == NULL) {
		ret = 1;
	}
	if (ret == 0 && asmc_add_op(&asmc, 0, ASMC_OP_NOP) == NULL) {
		ret = 1;
	}
	if (ret == 0) {
		mem_oom(1);
		ret = x86_parse_program_section(&bin, 0, len, mode, &asmc, ALLOC_STD);
		mem_oom(0);
	}

	asmc_free(&asmc);
	bin_free(&bin);
	return ret;
}

static int t_x86_fail_realloc(alloc_t *alloc, void **ptr, size_t *old_size, size_t new_size)
{
	(void)alloc;
	(void)ptr;
	(void)old_size;
	(void)new_size;
	return 1;
}

static size_t t_x86_build_linux_note(byte *buf)
{
	size_t off = 0;

	t_x86_put32(buf, off + 0, 4);
	t_x86_put32(buf, off + 4, 16);
	t_x86_put32(buf, off + 8, 1);
	mem_copy(buf + off + 12, 4, "GNU", 4);
	t_x86_put32(buf, off + 16, 0);
	t_x86_put32(buf, off + 20, 2);
	t_x86_put32(buf, off + 24, 3);
	t_x86_put32(buf, off + 28, 4);
	off += 32;

	t_x86_put32(buf, off + 0, 4);
	t_x86_put32(buf, off + 4, 16);
	t_x86_put32(buf, off + 8, 5);
	mem_copy(buf + off + 12, 4, "GNU", 4);
	t_x86_put32(buf, off + 16, 0xc0008002);
	t_x86_put32(buf, off + 20, 4);
	t_x86_put32(buf, off + 24, 0x1234);
	t_x86_put32(buf, off + 28, 0);
	off += 32;

	t_x86_put32(buf, off + 0, 4);
	t_x86_put32(buf, off + 4, 16);
	t_x86_put32(buf, off + 8, 5);
	mem_copy(buf + off + 12, 4, "GNU", 4);
	t_x86_put32(buf, off + 16, 0xc0000002);
	t_x86_put32(buf, off + 20, 4);
	t_x86_put32(buf, off + 24, 3);
	t_x86_put32(buf, off + 28, 0);
	off += 32;

	return off;
}

static elfc_sect_t *t_x86_add_sect(elfc_t *elfc, elfc_sect_type_t type, u64 addr, u64 size, strv_t label)
{
	elfc_sect_t *sect = arr_add(&elfc->sects, NULL);
	if (sect == NULL) {
		return NULL;
	}

	mem_set(sect, 0, sizeof(*sect));
	sect->type = type;
	sect->addr = addr;
	sect->size = size;

	if (label.data != NULL) {
		strvbuf_add(&elfc->strs, label, &sect->label);
	}

	return sect;
}

TEST(parse_x86_elfc_free)
{
	START;

	elfc_t elfc = {0};
	EXPECT_EQ(elfc_init(NULL, ALLOC_STD), NULL);
	EXPECT_EQ(elfc_init(&elfc, ALLOC_STD), &elfc);

	static const elfc_sect_type_t types[] = {
		ELF_SECT_TYPE_BYTES,
		ELF_SECT_TYPE_MAGIC,
		ELF_SECT_TYPE_PROGRAM_HEADER,
		ELF_SECT_TYPE_SECTION_HEADER,
		ELF_SECT_TYPE_INTERP,
		ELF_SECT_TYPE_NOTE,
		ELF_SECT_TYPE_STRTAB,
		ELF_SECT_TYPE_DYNAMIC,
		ELF_SECT_TYPE_DYNSYM,
		ELF_SECT_TYPE_RELADYN,
		ELF_SECT_TYPE_PROGRAM,
		ELF_SECT_TYPE_SECTION,
		ELF_SECT_TYPE_UNKNOWN,
	};

	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		elfc_sect_t *sect = arr_add(&elfc.sects, NULL);
		EXPECT_NE(sect, NULL);
		if (sect != NULL) {
			mem_set(sect, 0, sizeof(*sect));
			sect->type = types[i];
		}
	}

	elfc_free(NULL);
	elfc_free(&elfc);

	END;
}

TEST(parse_x86_elfc_asmc_manual)
{
	START;

	elfc_t elfc = {0};
	elfc_init(&elfc, ALLOC_STD);

	const u8 data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x42, 0x43};
	EXPECT_EQ(buf_resize(&elfc.bytes, sizeof(data)), 0);
	elfc.bytes.used = sizeof(data);
	mem_copy(elfc.bytes.data, sizeof(data), data, sizeof(data));

	const strv_t bytes_name = STRVT("bytes");
	t_x86_add_sect(&elfc, ELF_SECT_TYPE_BYTES, 5, 3, bytes_name);

	asmc_t asmc = {0};
	asmc_init(&asmc, 8, ALLOC_STD);
	EXPECT_EQ(elfc_asmc(&elfc, &asmc), 0);
	EXPECT_GT(asmc.ops.cnt, 0);

	asmc_op_t *op = arr_get(&asmc.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_LABEL);
	}

	asmc_free(&asmc);
	elfc_free(&elfc);

	END;
}

TEST(parse_bin_x86_parse)
{
	START;

	format_driver_t *format = format_driver_find(STRV("elf"));
	arch_driver_t *arch	= arch_driver_find(STRV("x86"));
	EXPECT_NE(format, NULL);
	EXPECT_NE(arch, NULL);

	if (format != NULL && arch != NULL) {
		asmc_t asmc = {0};
		asmc_init(&asmc, 2, ALLOC_STD);
		bin_t bin = {0};
		bin_init(&bin, 0xA00, ALLOC_STD);

		EXPECT_NE(format->parse(NULL, &bin, NULL, DST_NONE(), ALLOC_STD), 0);
		EXPECT_NE(t_x86_parse_elf_bin(NULL, &asmc, ALLOC_STD), 0);
		EXPECT_NE(t_x86_parse_elf_bin(&bin, NULL, ALLOC_STD), 0);

		bin_free(&bin);
		asmc_free(&asmc);
	}
	END;
}

TEST(parse_x86_parse_self)
{
	START;

	bin_t bin = {0};
	bin_init(&bin, 0xA00, ALLOC_STD);
	asmc_t asmc = {0};
	asmc_init(&asmc, 64, ALLOC_STD);

	EXPECT_NE(t_x86_parse_elf_bin(&bin, &asmc, ALLOC_STD), 0);

	bin_free(&bin);
	asmc_free(&asmc);

	END;
}

TEST(parse_x86_helpers_reg_lookup)
{
	START;

	EXPECT_EQ(x86_read_reg64(0), ASMC_REG_RAX);
	EXPECT_EQ(x86_read_reg64(1), ASMC_REG_RCX);
	EXPECT_EQ(x86_read_reg64(2), ASMC_REG_RDX);
	EXPECT_EQ(x86_read_reg64(3), ASMC_REG_RBX);
	EXPECT_EQ(x86_read_reg64(4), ASMC_REG_RSP);
	EXPECT_EQ(x86_read_reg64(5), ASMC_REG_RBP);
	EXPECT_EQ(x86_read_reg64(6), ASMC_REG_RSI);
	EXPECT_EQ(x86_read_reg64(7), ASMC_REG_RDI);
	EXPECT_EQ(x86_read_reg64(8), ASMC_REG_R8);
	EXPECT_EQ(x86_read_reg64(9), ASMC_REG_R9);
	log_set_quiet(0, 1);
	EXPECT_EQ(x86_read_reg64(16), ASMC_REG_UNKNOWN);
	EXPECT_EQ(x86_read_reg32(0), ASMC_REG_EAX);
	EXPECT_EQ(x86_read_reg32(1), ASMC_REG_ECX);
	EXPECT_EQ(x86_read_reg32(2), ASMC_REG_EDX);
	EXPECT_EQ(x86_read_reg32(3), ASMC_REG_EBX);
	EXPECT_EQ(x86_read_reg32(5), ASMC_REG_EBP);
	EXPECT_EQ(x86_read_reg32(16), ASMC_REG_UNKNOWN);
	log_set_quiet(0, 0);

	END;
}

TEST(parse_x86_helpers_reg_coverage)
{
	START;

	log_set_quiet(0, 1);
	EXPECT_EQ(x86_reg(0xFF, 8), ASMC_REG_UNKNOWN);
	EXPECT_EQ(x86_reg(0xFF, 16), ASMC_REG_UNKNOWN);
	EXPECT_EQ(x86_reg(0x01, 7), ASMC_REG_UNKNOWN);
	log_set_quiet(0, 0);

	END;
}

TEST(parse_x86_helpers_value_readers)
{
	START;

	byte one[] = {0xAB};
	bin_t bin  = {0};
	bin_init(&bin, sizeof(one), ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, one, sizeof(one)), 0);

	byte b	   = 0;
	size_t off = 0;
	EXPECT_EQ(x86_read_byte(&bin, &b, &off), 0);
	EXPECT_EQ(b, 0xAB);

	u64 val = 0;
	off	= 0;
	EXPECT_EQ(x86_read_val(&bin, &val, 1, &off), 0);
	EXPECT_EQ(val, 0xAB);
	off = 0;
	EXPECT_NE(x86_read_val(&bin, &val, 2, &off), 0);
	EXPECT_NE(x86_read_val(NULL, &val, 1, &off), 0);
	EXPECT_NE(x86_read_val(&bin, NULL, 1, &off), 0);
	EXPECT_NE(x86_read_val(&bin, &val, 1, NULL), 0);

	bin_free(&bin);
	log_set_quiet(0, 1);
	bin_init(&bin, 0, ALLOC_STD);
	EXPECT_NE(x86_read_byte(&bin, &b, &off), 0);
	log_set_quiet(0, 0);

	bin_free(&bin);

	END;
}

TEST(parse_x86_helpers_signed_and_relative_readers)
{
	START;

	s64 sval       = 0;
	byte signed1[] = {0xFE};
	bin_t bin      = {0};
	bin_init(&bin, sizeof(signed1), ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, signed1, sizeof(signed1)), 0);
	size_t off = 0;
	EXPECT_EQ(x86_read_signed_val(&bin, &sval, 1, &off), 0);
	EXPECT_EQ(sval, -2);
	byte signed2[] = {0xFE, 0xFF};
	bin_free(&bin);
	bin_init(&bin, sizeof(signed2), ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, signed2, sizeof(signed2)), 0);
	off = 0;
	EXPECT_EQ(x86_read_signed_val(&bin, &sval, 2, &off), 0);
	EXPECT_EQ(sval, -2);
	byte signed4[] = {0xFC, 0xFF, 0xFF, 0xFF};
	bin_free(&bin);
	bin_init(&bin, sizeof(signed4), ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, signed4, sizeof(signed4)), 0);
	off = 0;
	EXPECT_EQ(x86_read_signed_val(&bin, &sval, 4, &off), 0);
	EXPECT_EQ(sval, -4);
	byte signed8[] = {0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	bin_free(&bin);
	bin_init(&bin, sizeof(signed8), ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, signed8, sizeof(signed8)), 0);
	off = 0;
	EXPECT_EQ(x86_read_signed_val(&bin, &sval, 8, &off), 0);
	EXPECT_EQ(sval, -8);
	byte signed_fail[] = {0xFE, 0xFF};
	bin_free(&bin);
	bin_init(&bin, sizeof(signed_fail), ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, signed_fail, sizeof(signed_fail)), 0);
	off = 0;
	EXPECT_NE(x86_read_signed_val(&bin, &sval, 3, &off), 0);

	bin_free(&bin);
	EXPECT_NE(bin_init(&bin, 1, ALLOC_STD), NULL);
	asmc_oper_t oper = {0};
	off		 = 0;
	EXPECT_NE(x86_read_imm(&bin, 2, &off, 8, &oper), 0);
	off = 0;
	EXPECT_NE(x86_read_rel(&bin, 2, &off, 8, &oper), 0);

	bin_free(&bin);

	END;
}

TEST(parse_x86_helpers_elfc_parse_errors)
{
	START;

	elfc_t elfc = {0};
	elfc_init(&elfc, ALLOC_STD);
	size_t off = 0;

	log_set_quiet(0, 1);
	elfc_sect_t *sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_ELF_HEADER;
		EXPECT_NE(elfc_parse_header(&elfc, &off, 0, sect, ALLOC_STD), 0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_PROGRAM_HEADER;
		EXPECT_NE(elfc_parse_program_header(&elfc, 0, 0, 0, 0, sect, ALLOC_STD), 0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_SECTION_HEADER;
		EXPECT_NE(elfc_parse_section_header(&elfc, 0, 0, 0, 0, 0, sect, ALLOC_STD), 0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_DYNAMIC;
		EXPECT_NE(elfc_parse_dynamic_section(&elfc, 0, 0, 0, sect, ALLOC_STD), 0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_DYNSYM;
		EXPECT_NE(elfc_parse_dynsym_section(&elfc, 0, 0, 0, 0, sect, ALLOC_STD), 0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_RELADYN;
		EXPECT_NE(elfc_parse_reladyn_section(&elfc, 0, 0, 0, 0, sect, ALLOC_STD), 0);
	}
	log_set_quiet(0, 0);

	elfc_free(&elfc);

	END;
}

TEST(parse_x86_elfc_init_oom)
{
	START;

	elfc_t elfc = {0};
	mem_oom(1);
	EXPECT_EQ(elfc_init(&elfc, ALLOC_STD), NULL);
	mem_oom(0);

	END;
}

TEST(parse_x86_parse_elf_layouts_32)
{
	START;

	byte image[0x200] = {0};
	elfc_t elfc	  = {0};
	elfc_init(&elfc, ALLOC_STD);
	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, image, sizeof(image)), 0);

	size_t off = 0;

	elfc_sect_t *sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_ELF_HEADER;
		EXPECT_EQ(elfc_parse_header(&elfc, &off, 1, sect, ALLOC_STD), 0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_PROGRAM_HEADER;
		EXPECT_EQ(elfc_parse_program_header(&elfc, 0, 1, 1, 32, sect, ALLOC_STD), 0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_SECTION_HEADER;
		EXPECT_EQ(elfc_parse_section_header(&elfc, 0, 1, 1, 40, 0, sect, ALLOC_STD), 0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_DYNAMIC;
		EXPECT_EQ(elfc_parse_dynamic_section(&elfc, 0, 16, 1, sect, ALLOC_STD), 0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_DYNSYM;
		EXPECT_EQ(elfc_parse_dynsym_section(&elfc, 0, 32, 1, 0, sect, ALLOC_STD), 0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_RELADYN;
		EXPECT_EQ(elfc_parse_reladyn_section(&elfc, 0, 12, 1, 4, sect, ALLOC_STD), 0);
	}

	elfc_free(&elfc);

	END;
}

TEST(parse_x86_parse_notes_and_tables)
{
	START;

	byte image[0x400] = {0};
	size_t note	  = 0;

	t_x86_put32(image, note + 0, 4);
	t_x86_put32(image, note + 4, 16);
	t_x86_put32(image, note + 8, 1);
	mem_copy(image + note + 12, 4, "GNU", 4);
	t_x86_put32(image, note + 16, 0);
	t_x86_put32(image, note + 20, 1);
	t_x86_put32(image, note + 24, 2);
	t_x86_put32(image, note + 28, 3);
	note += 32;

	t_x86_put32(image, note + 0, 4);
	t_x86_put32(image, note + 4, 20);
	t_x86_put32(image, note + 8, 3);
	mem_copy(image + note + 12, 4, "GNU", 4);
	for (size_t i = 0; i < 20; i++) {
		image[note + 16 + i] = (byte)(i + 1);
	}
	note += 36;

	t_x86_put32(image, note + 0, 4);
	t_x86_put32(image, note + 4, 16);
	t_x86_put32(image, note + 8, 3);
	mem_copy(image + note + 12, 4, "GNU", 4);
	for (size_t i = 0; i < 16; i++) {
		image[note + 16 + i] = (byte)(0x80 + i);
	}
	note += 32;

	t_x86_put32(image, note + 0, 4);
	t_x86_put32(image, note + 4, 40);
	t_x86_put32(image, note + 8, 5);
	mem_copy(image + note + 12, 4, "GNU", 4);
	t_x86_put32(image, note + 16, 0xc0000002);
	t_x86_put32(image, note + 20, 4);
	t_x86_put32(image, note + 24, 3);
	t_x86_put32(image, note + 28, 0);
	t_x86_put32(image, note + 32, 0xC0008002);
	t_x86_put32(image, note + 36, 4);
	t_x86_put32(image, note + 40, 1);
	t_x86_put32(image, note + 44, 0);
	t_x86_put32(image, note + 48, 0xDEADBEEF);
	t_x86_put32(image, note + 52, 0);
	note += 56;

	t_x86_put32(image, note + 0, 4);
	t_x86_put32(image, note + 4, 4);
	t_x86_put32(image, note + 8, 0x1234);
	mem_copy(image + note + 12, 4, "GNU", 4);
	t_x86_put32(image, note + 16, 0xDEADBEEF);
	note += 20;

	t_x86_put32(image, note + 0, 4);
	t_x86_put32(image, note + 4, 16);
	t_x86_put32(image, note + 8, 1);
	mem_copy(image + note + 12, 4, "GNU", 4);
	t_x86_put32(image, note + 16, 0);
	t_x86_put32(image, note + 20, 1);
	t_x86_put32(image, note + 24, 2);
	t_x86_put32(image, note + 28, 3);
	note += 32;

	t_x86_put32(image, note + 0, 4);
	t_x86_put32(image, note + 4, 16);
	t_x86_put32(image, note + 8, 3);
	mem_copy(image + note + 12, 4, "GNU", 4);
	t_x86_put32(image, note + 16, 0xC0008002);
	t_x86_put32(image, note + 20, 4);
	t_x86_put32(image, note + 24, 2);
	t_x86_put32(image, note + 28, 0);
	note += 32;

	elfc_t elfc = {0};
	elfc_init(&elfc, ALLOC_STD);
	bin_t bin = {0};
	bin_init(&bin, note, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, note), 0);
	mem_copy(elfc.bytes.data, note, image, note);
	elfc.bytes.used = note;

	elfc_sect_t *sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_NOTE;
	}

	log_set_quiet(0, 1);
	EXPECT_EQ(elfc_parse_note_section(&elfc, 0, note, sect, ALLOC_STD), 0);
	log_set_quiet(0, 0);
	EXPECT_GT(sect->data.note.notes.cnt, 0);

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_STRTAB;
	}

	const byte strtab[] = {'a', 0, 'b', 'c', 0, 0};
	buf_resize(&elfc.bytes, sizeof(strtab));
	elfc.bytes.used = sizeof(strtab);
	mem_copy(elfc.bytes.data, sizeof(strtab), strtab, sizeof(strtab));
	dst_t dst = DST_NONE();
	EXPECT_EQ(elfc_parse_strtab_section(&elfc, 0, sizeof(strtab), sect, &dst, ALLOC_STD), 0);
	EXPECT_GT(sect->data.strtab.strs.cnt, 0);

	elfc_free(&elfc);
	bin_free(&bin);

	END;
}

TEST(parse_x86_parse_notes_linux_isa)
{
	START;

	byte image[0x200] = {0};
	size_t note	  = 0;

	t_x86_put32(image, note + 0, 4);
	t_x86_put32(image, note + 4, 16);
	t_x86_put32(image, note + 8, 1);
	mem_copy(image + note + 12, 4, "GNU", 4);
	t_x86_put32(image, note + 16, 0);
	t_x86_put32(image, note + 20, 1);
	t_x86_put32(image, note + 24, 2);
	t_x86_put32(image, note + 28, 3);
	note += 32;

	elfc_t elfc = {0};
	elfc_init(&elfc, ALLOC_STD);
	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, image, note), 0);

	elfc_sect_t *sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_NOTE;
		EXPECT_EQ(elfc_parse_note_section(&elfc, 0, note, sect, ALLOC_STD), 0);
		EXPECT_EQ(sect->data.note.notes.cnt, 1);
	}
	elfc_free(&elfc);

	END;
}

TEST(parse_x86_parse_program_section_bounds)
{
	START;

	byte image[0x200] = {0};
	size_t len	  = t_x86_build_program_blob(image, sizeof(image));
	bin_t bin	  = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, len), 0);

	asmc_t asmc = {0};
	EXPECT_NE(x86_parse_program_section(NULL, 0, len, 2, &asmc, ALLOC_STD), 0);
	EXPECT_NE(x86_parse_program_section(&bin, len + 1, 1, 2, &asmc, ALLOC_STD), 0);
	EXPECT_NE(x86_parse_program_section(&bin, 0, len + 1, 2, &asmc, ALLOC_STD), 0);

	bin_free(&bin);

	END;
}

TEST(parse_x86_parse_program_section_init_oom)
{
	START;

	byte image[0x200] = {0};
	size_t len	  = t_x86_build_program_blob(image, sizeof(image));
	bin_t bin	  = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, len), 0);

	asmc_t asmc = {0};
	mem_oom(1);
	EXPECT_NE(x86_parse_program_section(&bin, 0, len, 2, &asmc, ALLOC_STD), 0);
	mem_oom(0);

	asmc_free(&asmc);
	bin_free(&bin);

	END;
}

TEST(parse_x86_parse_program_section_decode_oom)
{
	START;

	byte image[0x200] = {0};
	size_t len	  = t_x86_build_program_blob(image, sizeof(image));
	bin_t bin	  = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, len), 0);

	asmc_t asmc = {0};
	EXPECT_NE(asmc_init(&asmc, 8, ALLOC_STD), NULL);

	mem_oom(1);
	const byte oom_unknown[] = {0xAA};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_unknown), 2, &asmc, ALLOC_STD), 0);
	const byte oom_reg_rm[] = {0x01, 0xC0};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_reg_rm), 2, &asmc, ALLOC_STD), 0);
	const byte oom_group1[] = {0x80, 0x00, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_group1), 2, &asmc, ALLOC_STD), 0);
	const byte oom_shift[] = {0xC1, 0xE0, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_shift), 2, &asmc, ALLOC_STD), 0);
	const byte oom_mov_rm[] = {0xC7, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_mov_rm), 2, &asmc, ALLOC_STD), 0);
	const byte oom_ff[] = {0xFF, 0xC0};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_ff), 2, &asmc, ALLOC_STD), 0);
	const byte oom_ff_default[] = {0xFF, 0xD8};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_ff_default), 2, &asmc, ALLOC_STD), 0);
	const byte oom_ext_nop[] = {0x0F, 0x1F, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_ext_nop), 2, &asmc, ALLOC_STD), 0);
	const byte oom_ext_syscall[] = {0x0F, 0x05};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_ext_syscall), 2, &asmc, ALLOC_STD), 0);
	const byte oom_acc_imm[] = {0x05, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_acc_imm), 2, &asmc, ALLOC_STD), 0);
	const byte oom_jcc[] = {0x0F, 0x84, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_jcc), 2, &asmc, ALLOC_STD), 0);
	const byte oom_je[] = {0x74, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_je), 2, &asmc, ALLOC_STD), 0);
	const byte oom_push_reg[] = {0x50};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_push_reg), 2, &asmc, ALLOC_STD), 0);
	const byte oom_pop[] = {0x58};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_pop), 2, &asmc, ALLOC_STD), 0);
	const byte oom_nop[] = {0x90};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_nop), 2, &asmc, ALLOC_STD), 0);
	const byte oom_push_imm[] = {0x68, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_push_imm), 2, &asmc, ALLOC_STD), 0);
	const byte oom_mov_imm[] = {0xB8, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_mov_imm), 2, &asmc, ALLOC_STD), 0);
	const byte oom_call[] = {0xE8, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_call), 2, &asmc, ALLOC_STD), 0);
	const byte oom_jmp[] = {0xE9, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, sizeof(oom_jmp), 2, &asmc, ALLOC_STD), 0);
	mem_oom(0);

	asmc_free(&asmc);
	bin_free(&bin);

	END;
}

TEST(parse_x86_parse_program_section_address_truncation)
{
	START;

	byte image[0x200]  = {0};
	size_t len	   = t_x86_build_program_blob(image, sizeof(image));
	const byte extra[] = {
		0x01, 0x00, 0x29, 0x00, 0x39, 0x00, 0x85, 0x00, 0x83, 0xC8, 0x00, 0x8B, 0xC0, 0x8D, 0xC0, 0xC1,
		0xC0, 0x00, 0xC6, 0xC0, 0x00, 0xC7, 0xC8, 0x00, 0xD1, 0xC0, 0xFF, 0x00, 0xF3, 0x0F, 0xAA,
	};
	mem_copy(image + len, sizeof(extra), extra, sizeof(extra));
	len += sizeof(extra);

	bin_t bin = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, len), 0);

	asmc_t asmc = {0};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, len, 2, &asmc, ALLOC_STD), 0);
	EXPECT_GT(asmc.ops.cnt, 0);

	const byte bad_modrm[] = {0x01};
	EXPECT_EQ(t_x86_parse_program_blob(bad_modrm, sizeof(bad_modrm), 2, NULL), 0);

	const byte bad_sib[] = {0x01, 0x04};
	EXPECT_EQ(t_x86_parse_program_blob(bad_sib, sizeof(bad_sib), 2, NULL), 0);

	const byte bad_disp32[] = {0x01, 0x04, 0x25};
	EXPECT_EQ(t_x86_parse_program_blob(bad_disp32, sizeof(bad_disp32), 2, NULL), 0);

	const byte bad_disp8[] = {0x01, 0x45};
	EXPECT_EQ(t_x86_parse_program_blob(bad_disp8, sizeof(bad_disp8), 2, NULL), 0);

	const byte bad_disp8_short[] = {0x01, 0x40};
	EXPECT_EQ(t_x86_parse_program_blob(bad_disp8_short, sizeof(bad_disp8_short), 2, NULL), 0);

	const byte bad_disp8_sib[] = {0x01, 0x44, 0x24};
	EXPECT_EQ(t_x86_parse_program_blob(bad_disp8_sib, sizeof(bad_disp8_sib), 2, NULL), 0);

	const byte bad_disp32_base[] = {0x01, 0x05};
	EXPECT_EQ(t_x86_parse_program_blob(bad_disp32_base, sizeof(bad_disp32_base), 2, NULL), 0);

	const byte bad_disp32_direct[] = {0x01, 0x85};
	EXPECT_EQ(t_x86_parse_program_blob(bad_disp32_direct, sizeof(bad_disp32_direct), 2, NULL), 0);

	const byte bad_disp32_sib[] = {0x01, 0x84, 0x24};
	EXPECT_EQ(t_x86_parse_program_blob(bad_disp32_sib, sizeof(bad_disp32_sib), 2, NULL), 0);

	asmc_free(&asmc);
	bin_free(&bin);

	END;
}

TEST(parse_x86_parse_program_section_opcode_truncation)
{
	START;

	byte image[0x200]  = {0};
	size_t len	   = t_x86_build_program_blob(image, sizeof(image));
	const byte extra[] = {
		0x01, 0x00, 0x29, 0x00, 0x39, 0x00, 0x85, 0x00, 0x83, 0xC8, 0x00, 0x8B, 0xC0, 0x8D, 0xC0, 0xC1,
		0xC0, 0x00, 0xC6, 0xC0, 0x00, 0xC7, 0xC8, 0x00, 0xD1, 0xC0, 0xFF, 0x00, 0xF3, 0x0F, 0xAA,
	};
	mem_copy(image + len, sizeof(extra), extra, sizeof(extra));
	len += sizeof(extra);

	bin_t bin = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, len), 0);

	asmc_t asmc = {0};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, len, 2, &asmc, ALLOC_STD), 0);
	EXPECT_GT(asmc.ops.cnt, 0);

	const byte bad_group1_modrm[] = {0x80};
	EXPECT_EQ(t_x86_parse_program_blob(bad_group1_modrm, sizeof(bad_group1_modrm), 2, NULL), 0);

	const byte bad_group1_imm[] = {0x80, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob(bad_group1_imm, sizeof(bad_group1_imm), 2, NULL), 0);

	const byte bad_shift_modrm[] = {0xC1};
	EXPECT_EQ(t_x86_parse_program_blob(bad_shift_modrm, sizeof(bad_shift_modrm), 2, NULL), 0);

	const byte bad_shift_imm[] = {0xC1, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob(bad_shift_imm, sizeof(bad_shift_imm), 2, NULL), 0);

	const byte bad_mov_imm_modrm[] = {0xC7};
	EXPECT_EQ(t_x86_parse_program_blob(bad_mov_imm_modrm, sizeof(bad_mov_imm_modrm), 2, NULL), 0);

	const byte bad_mov_imm[] = {0xC7, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob(bad_mov_imm, sizeof(bad_mov_imm), 2, NULL), 0);

	asmc_free(&asmc);
	bin_free(&bin);

	END;
}

TEST(parse_x86_parse_program_section_control_truncation)
{
	START;

	byte image[0x200]  = {0};
	size_t len	   = t_x86_build_program_blob(image, sizeof(image));
	const byte extra[] = {
		0x01, 0x00, 0x29, 0x00, 0x39, 0x00, 0x85, 0x00, 0x83, 0xC8, 0x00, 0x8B, 0xC0, 0x8D, 0xC0, 0xC1,
		0xC0, 0x00, 0xC6, 0xC0, 0x00, 0xC7, 0xC8, 0x00, 0xD1, 0xC0, 0xFF, 0x00, 0xF3, 0x0F, 0xAA,
	};
	mem_copy(image + len, sizeof(extra), extra, sizeof(extra));
	len += sizeof(extra);

	bin_t bin = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, len), 0);

	asmc_t asmc = {0};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, len, 2, &asmc, ALLOC_STD), 0);
	EXPECT_GT(asmc.ops.cnt, 0);

	const byte bad_ff[] = {0xFF};
	EXPECT_EQ(t_x86_parse_program_blob(bad_ff, sizeof(bad_ff), 2, NULL), 0);

	const byte bad_ext[] = {0x0F};
	EXPECT_EQ(t_x86_parse_program_blob(bad_ext, sizeof(bad_ext), 2, NULL), 0);

	const byte bad_ext_nop[] = {0x0F, 0x1F};
	EXPECT_EQ(t_x86_parse_program_blob(bad_ext_nop, sizeof(bad_ext_nop), 2, NULL), 0);

	const byte bad_ext_jcc[] = {0x0F, 0x84};
	EXPECT_EQ(t_x86_parse_program_blob(bad_ext_jcc, sizeof(bad_ext_jcc), 2, NULL), 0);

	const byte bad_call[] = {0xE8};
	EXPECT_EQ(t_x86_parse_program_blob(bad_call, sizeof(bad_call), 2, NULL), 0);

	const byte bad_jmp[] = {0xE9};
	EXPECT_EQ(t_x86_parse_program_blob(bad_jmp, sizeof(bad_jmp), 2, NULL), 0);

	const byte bad_push_imm[] = {0x68};
	EXPECT_EQ(t_x86_parse_program_blob(bad_push_imm, sizeof(bad_push_imm), 2, NULL), 0);

	const byte bad_mov_imm_reg[] = {0xB8};
	EXPECT_EQ(t_x86_parse_program_blob(bad_mov_imm_reg, sizeof(bad_mov_imm_reg), 2, NULL), 0);

	asmc_free(&asmc);
	bin_free(&bin);

	END;
}

TEST(parse_x86_parse_decode_edge_coverage)
{
	START;

	asmc_t asmc = {0};
	EXPECT_EQ(asmc_init(&asmc, 4, ALLOC_STD), &asmc);

	const byte only_prefix[] = {0x66};
	EXPECT_EQ(t_x86_parse_program_blob(only_prefix, sizeof(only_prefix), 1, NULL), 0);

	bin_t bin = {0};
	EXPECT_NE(bin_init(&bin, 1, ALLOC_STD), NULL);
	bin.buf.used = 1;
	void *saved  = bin.buf.data;
	bin.buf.data = NULL;
	EXPECT_EQ(x86_parse_program_section(&bin, 0, 1, REVERSE_IMAGE_DATA_LE, &asmc, ALLOC_STD), 0);
	bin.buf.data = saved;

	asmc_free(&asmc);
	bin_free(&bin);

	END;
}

TEST(parse_x86_parse_add_op_failure_coverage)
{
	START;

	const byte op_unknown[] = {0xAA};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_unknown, sizeof(op_unknown), 1), 0);

	const byte op_group1[] = {0x80, 0xC0, 0x01};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_group1, sizeof(op_group1), 1), 0);

	const byte op_shift[] = {0xC1, 0xE0, 0x01};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_shift, sizeof(op_shift), 1), 0);

	const byte op_mov_imm_rm[] = {0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_mov_imm_rm, sizeof(op_mov_imm_rm), 1), 0);

	const byte op_ff_default[] = {0xFF, 0xD8};
	EXPECT_EQ(t_x86_parse_program_blob(op_ff_default, sizeof(op_ff_default), 1, NULL), 0);
	const byte op_ff[] = {0xFF, 0xC0};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_ff, sizeof(op_ff), 1), 0);

	const byte op_ext_nop[] = {0x0F, 0x1F, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_ext_nop, sizeof(op_ext_nop), 1), 0);

	const byte op_ext_jcc[] = {0x0F, 0x84, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_ext_jcc, sizeof(op_ext_jcc), 1), 0);

	const byte op_acc_imm[] = {0x05, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_acc_imm, sizeof(op_acc_imm), 1), 0);

	const byte op_push_reg[] = {0x50};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_push_reg, sizeof(op_push_reg), 1), 0);

	const byte op_pop_reg[] = {0x58};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_pop_reg, sizeof(op_pop_reg), 1), 0);

	const byte op_push_imm[] = {0x68, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_push_imm, sizeof(op_push_imm), 1), 0);

	const byte op_jcc8[] = {0x74, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_jcc8, sizeof(op_jcc8), 1), 0);

	const byte op_nop[] = {0x90};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_nop, sizeof(op_nop), 1), 0);

	const byte op_mov_imm[] = {0xB8, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_mov_imm, sizeof(op_mov_imm), 1), 0);

	const byte op_call[] = {0xE8, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_call, sizeof(op_call), 1), 0);

	const byte op_jmp[] = {0xE9, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(t_x86_parse_program_blob_add_op_failure(op_jmp, sizeof(op_jmp), 1), 0);

	END;
}

TEST(parse_x86_parse_program_section)
{
	START;

	byte image[0x200] = {0};
	size_t len	  = t_x86_build_program_blob(image, sizeof(image));
	EXPECT_GT(len, 0);

	bin_t bin = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, len), 0);

	asmc_t asmc = {0};
	EXPECT_EQ(x86_parse_program_section(&bin, 0, len, 1, &asmc, ALLOC_STD), 0);
	EXPECT_GT(asmc.ops.cnt, 0);

	asmc_op_t *op = arr_get(&asmc.ops, 0);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		EXPECT_EQ(op->type, ASMC_OP_ADD);
	}

	asmc_free(&asmc);
	bin_free(&bin);

	END;
}

TEST(parse_x86_parse_full_image)
{
	START;

	byte image[0x1200] = {0};
	size_t len	   = t_x86_build_elf_full(image, sizeof(image));
	EXPECT_GT(len, 0);

	elfc_t elfc = {0};
	elfc_init(&elfc, ALLOC_STD);
	reverse_image_t parsed = {0};
	reverse_image_init(&parsed, ALLOC_STD);
	bin_t bin = {0};
	bin_init(&bin, len, ALLOC_STD);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, image, len), 0);
	mem_copy(elfc.bytes.data, len, image, len);
	elfc.bytes.used = len;

	log_set_quiet(0, 1);
	EXPECT_EQ(elfc_parse(&elfc, &parsed, DST_NONE(), ALLOC_STD), 0);
	log_set_quiet(0, 0);
	EXPECT_EQ(parsed.sections.cnt, 11);

	reverse_image_section_t *text_section = arr_get(&parsed.sections, 8);
	EXPECT_NE(text_section, NULL);
	if (text_section != NULL) {
		EXPECT_EQ(text_section->index, 8);
		EXPECT_EQ(strv_eq(text_section->name, STRV(".text")), 1);
		EXPECT_EQ(text_section->type, 1);
		EXPECT_EQ(text_section->format_flags, 1 << 2);
		EXPECT_EQ(text_section->off, 0x840);
		EXPECT_EQ(text_section->size, 15);
		EXPECT_EQ(text_section->align, 16);
		EXPECT_EQ(text_section->flags & REVERSE_IMAGE_SECTION_EXEC, REVERSE_IMAGE_SECTION_EXEC);
	}

	reverse_image_section_t *dynamic_section = arr_get(&parsed.sections, 5);
	EXPECT_NE(dynamic_section, NULL);
	if (dynamic_section != NULL) {
		EXPECT_EQ(strv_eq(dynamic_section->name, STRV(".dynamic")), 1);
		EXPECT_EQ(dynamic_section->type, 6);
		EXPECT_EQ(dynamic_section->link, 4);
		EXPECT_EQ(dynamic_section->entry_size, 16);
		EXPECT_EQ(dynamic_section->flags & REVERSE_IMAGE_SECTION_EXEC, 0);
	}

	const byte bytes_data[] = {0x11, 0x22, 0x33, 0x44};
	mem_copy((byte *)elfc.bytes.data + 0x980, sizeof(bytes_data), bytes_data, sizeof(bytes_data));
	const strv_t bytes_label = STRVT("manual_bytes");
	elfc_sect_t *bytes_sect	 = t_x86_add_sect(&elfc, ELF_SECT_TYPE_BYTES, 0x980, sizeof(bytes_data), bytes_label);
	EXPECT_NE(bytes_sect, NULL);
	const strv_t unknown_label = STRVT("manual_unknown");
	elfc_sect_t *unknown_sect  = t_x86_add_sect(&elfc, ELF_SECT_TYPE_UNKNOWN, 0x990, 1, unknown_label);
	EXPECT_NE(unknown_sect, NULL);
	if (unknown_sect != NULL) {
		*(byte *)buf_get(&elfc.bytes, 0x990) = 0xAA;
	}

	EXPECT_NE(elfc_find_sect(&elfc, 0x980), NULL);
	EXPECT_NE(elfc_find_sect(&elfc, 0x990), NULL);
	EXPECT_EQ(elfc_find_sect(&elfc, 0x12345678), NULL);

	asmc_t asmc = {0};
	asmc_init(&asmc, 512, ALLOC_STD);
	log_set_quiet(0, 1);
	EXPECT_EQ(elfc_asmc(&elfc, &asmc), 0);
	log_set_quiet(0, 0);
	EXPECT_GT(asmc.ops.cnt, 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(t_x86_parse_elf_bin(&bin, &asmc, ALLOC_STD), 0);
	log_set_quiet(0, 0);

	uint unknown_zeros = 4;
	EXPECT_EQ(elfc_asmc_unknown_zeros(&asmc, &unknown_zeros), 0);
	EXPECT_EQ(unknown_zeros, 0);
	unknown_zeros = 2;
	EXPECT_EQ(elfc_asmc_unknown_zeros(&asmc, &unknown_zeros), 0);
	EXPECT_EQ(unknown_zeros, 0);

	asmc_free(&asmc);
	bin_free(&bin);
	reverse_image_free(&parsed);
	elfc_free(&elfc);

	END;
}

TEST(parse_x86_parse_full_image_unknown_machine_data)
{
	START;

	byte image[0x1200] = {0};
	size_t len	   = t_x86_build_elf_full(image, sizeof(image));
	EXPECT_GT(len, 0);

	image[5] = 0xFF;
	t_x86_put16(image, 18, 0xFFFF);

	elfc_t elfc = {0};
	EXPECT_EQ(elfc_init(&elfc, ALLOC_STD), &elfc);
	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, image, len), 0);

	reverse_image_t parsed = {0};
	EXPECT_NE(reverse_image_init(&parsed, ALLOC_STD), NULL);

	log_set_quiet(0, 1);
	EXPECT_EQ(elfc_parse(&elfc, &parsed, DST_NONE(), ALLOC_STD), 0);
	log_set_quiet(0, 0);
	EXPECT_EQ(parsed.machine, REVERSE_IMAGE_MACHINE_UNKNOWN);
	EXPECT_GT(parsed.sections.cnt, 0);

	reverse_image_section_t *section = arr_get(&parsed.sections, 0);
	EXPECT_NE(section, NULL);
	if (section != NULL) {
		EXPECT_EQ(section->data, REVERSE_IMAGE_DATA_UNKNOWN);
	}

	elfc_free(&elfc);
	reverse_image_free(&parsed);

	END;
}

TEST(parse_x86_parse_full_image_big_endian_note_unknowns)
{
	START;

	byte image[0x1200] = {0};
	size_t len	   = t_x86_build_elf_full(image, sizeof(image));
	EXPECT_GT(len, 0);

	image[5] = ELFC_IDENT_DATA_BE;
	t_x86_put32(image, 0x540 + 16, 99);
	t_x86_put32(image, 0x5A4 + 40, 2);

	elfc_t elfc = {0};
	EXPECT_EQ(elfc_init(&elfc, ALLOC_STD), &elfc);
	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, image, len), 0);

	reverse_image_t parsed = {0};
	EXPECT_NE(reverse_image_init(&parsed, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(elfc_parse(&elfc, &parsed, DST_NONE(), ALLOC_STD), 0);
	log_set_quiet(0, 0);

	reverse_image_section_t *section = arr_get(&parsed.sections, 0);
	EXPECT_NE(section, NULL);
	if (section != NULL) {
		EXPECT_EQ(section->data, REVERSE_IMAGE_DATA_BE);
	}

	asmc_t asmc = {0};
	EXPECT_NE(asmc_init(&asmc, 256, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(elfc_asmc(&elfc, &asmc), 0);
	log_set_quiet(0, 0);
	asmc_free(&asmc);

	elfc_sect_t *sect = NULL;
	uint i		   = 0;
	arr_foreach(&elfc.sects, i, sect)
	{
		if (sect->type != ELF_SECT_TYPE_NOTE) {
			continue;
		}
		note_section_note_t *note = NULL;
		uint note_i		  = 0;
		arr_foreach(&sect->data.note.notes, note_i, note)
		{
			if (note->type != NOTE_TYPE_GNU_PROPERTIES) {
				continue;
			}
			note_section_gnu_property_t *gnu_property = arr_get(&note->data.gnu_properties.arr, 0);
			if (gnu_property == NULL) {
				continue;
			}
			gnu_property->type = NOTE_SECTION_GNU_PROPERTY_UNKNOWN;
			goto done_mutation;
		}
	}
done_mutation:

	EXPECT_NE(asmc_init(&asmc, 256, ALLOC_STD), NULL);
	log_set_quiet(0, 1);
	EXPECT_EQ(elfc_asmc(&elfc, &asmc), 0);
	log_set_quiet(0, 0);
	asmc_free(&asmc);

	elfc_free(&elfc);
	reverse_image_free(&parsed);

	END;
}

TEST(parse_x86_parse_full_image_add_image_section_failure)
{
	START;

	byte image[0x1200] = {0};
	size_t len	   = t_x86_build_elf_full(image, sizeof(image));
	EXPECT_GT(len, 0);

	elfc_t elfc = {0};
	EXPECT_EQ(elfc_init(&elfc, ALLOC_STD), &elfc);
	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, image, len), 0);

	reverse_image_t parsed = {0};
	EXPECT_NE(reverse_image_init(&parsed, ALLOC_STD), NULL);
	parsed.sections.alloc.realloc = t_x86_fail_realloc;

	log_set_quiet(0, 1);
	EXPECT_NE(elfc_parse(&elfc, &parsed, DST_NONE(), ALLOC_STD), 0);
	log_set_quiet(0, 0);

	elfc_free(&elfc);
	reverse_image_free(&parsed);

	END;
}

TEST(parse_bin_x86_null)
{
	START;
	asmc_t asmc = {0};
	EXPECT_NE(t_x86_parse_elf_bin(NULL, NULL, ALLOC_STD), 0);
	EXPECT_NE(t_x86_parse_elf_bin(NULL, &asmc, ALLOC_STD), 0);
	END;
}

TEST(parse_x86_asmc_schema)
{
	START;

	asmc_t asmc = {0};
	asmc_init(&asmc, 16, ALLOC_STD);

	schema_t schema	      = {0};
	const strv_t labels[] = {STRVT("zero")};
	int dummy	      = 0;
	log_set_quiet(0, 1);
	EXPECT_EQ(elfc_asmc_schema(&asmc, &schema, labels, &dummy, 0, 0, 0), 0);
	log_set_quiet(0, 0);

	elfc_t elfc = {0};
	elfc_init(&elfc, ALLOC_STD);
	byte image[0x200] = {0};
	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, image, sizeof(image)), 0);

	elfc_sect_t *sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_ELF_IDENT;
		size_t off = 0;
		EXPECT_EQ(elfc_parse_ident(&elfc, &off, sect, ALLOC_STD), 0);
		const strv_t ident_labels[] = {
			STRVT("ident_class"),
			STRVT("ident_data"),
			STRVT("ident_version"),
			STRVT("ident_osabi"),
			STRVT("ident_abiversion"),
			STRVT("ident_pad"),
		};
		EXPECT_EQ(elfc_asmc_schema(&asmc,
					   &sect->data.elf_ident.schema,
					   ident_labels,
					   sect->data.elf_ident.data,
					   sect->data.elf_ident.layout,
					   1,
					   7),
			  0);
	}

	sect = arr_add(&elfc.sects, NULL);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		mem_set(sect, 0, sizeof(*sect));
		sect->type = ELF_SECT_TYPE_ELF_HEADER;
		size_t off = 0;
		EXPECT_EQ(elfc_parse_header(&elfc, &off, 2, sect, ALLOC_STD), 0);
		const strv_t header_labels[] = {
			STRVT("header_type"),
			STRVT("header_machine"),
			STRVT("header_version"),
			STRVT("header_entry"),
			STRVT("header_phoff"),
			STRVT("header_shoff"),
			STRVT("header_flags"),
			STRVT("header_ehsize"),
			STRVT("header_phentsize"),
			STRVT("header_phnum"),
			STRVT("header_shentsize"),
			STRVT("header_shnum"),
			STRVT("header_shstrndx"),
		};
		EXPECT_EQ(elfc_asmc_schema(&asmc,
					   &sect->data.elf_header.schema,
					   header_labels,
					   sect->data.elf_header.data,
					   sect->data.elf_header.layout,
					   0,
					   0),
			  0);
	}

	asmc_free(&asmc);
	elfc_free(&elfc);

	END;
}

TEST(parse_x86_map_name_oom)
{
	START;

	elfc_t elfc = {0};
	elfc_init(&elfc, ALLOC_STD);

	byte image[0x100] = {0};
	size_t dynstr_off = 0x40;
	mem_copy(image + dynstr_off, 10, "libc.so.6", 10);
	size_t len = 0x60;
	t_x86_put64(image, 0x00, 0x1);
	t_x86_put64(image, 0x08, 0x01);
	t_x86_put64(image, 0x10, 0x5);
	t_x86_put64(image, 0x18, dynstr_off);
	t_x86_put64(image, 0x20, 0x0);
	t_x86_put64(image, 0x28, 0x0);

	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, image, len), 0);

	elfc_sect_t *dynamic = arr_add(&elfc.sects, NULL);
	EXPECT_NE(dynamic, NULL);
	if (dynamic != NULL) {
		mem_set(dynamic, 0, sizeof(*dynamic));
		dynamic->type = ELF_SECT_TYPE_DYNAMIC;
		EXPECT_EQ(elfc_parse_dynamic_section(&elfc, 0, 48, 2, dynamic, ALLOC_STD), 0);
		const u64 *val = tbl_get_cell(&dynamic->data.dynamic.tbl, 0, 1);
		EXPECT_NE(val, NULL);
		if (val != NULL) {
			u64 empty_val = 0;
			log_set_quiet(0, 1);
			EXPECT_EQ(elfc_map_dynamic_name(
					  &dynamic->data.dynamic.tbl, 0, 99, &empty_val, (void *)((byte *)elfc.bytes.data + dynstr_off)),
				  1);
			log_set_quiet(0, 0);
		}
	}

	elfc_sect_t *dynsym = arr_add(&elfc.sects, NULL);
	EXPECT_NE(dynsym, NULL);
	if (dynsym != NULL) {
		mem_set(dynsym, 0, sizeof(*dynsym));
		dynsym->type = ELF_SECT_TYPE_DYNSYM;

		byte dynsym_bytes[0x80] = {0};
		mem_copy(dynsym_bytes + dynstr_off, 12, "\0puts\0alpha\0", 12);
		t_x86_put32(dynsym_bytes, 0x00, 1);
		dynsym_bytes[0x04] = 0x12;
		dynsym_bytes[0x05] = 0x00;
		t_x86_put16(dynsym_bytes, 0x06, 0);
		t_x86_put64(dynsym_bytes, 0x08, 0);
		t_x86_put64(dynsym_bytes, 0x10, 4);
		t_x86_put32(dynsym_bytes, 0x18, 6);
		dynsym_bytes[0x1C] = 0x12;
		dynsym_bytes[0x1D] = 0x00;
		t_x86_put16(dynsym_bytes, 0x1E, 0);
		t_x86_put64(dynsym_bytes, 0x20, 0x401000);
		t_x86_put64(dynsym_bytes, 0x28, 4);
		EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, dynsym_bytes, sizeof(dynsym_bytes)), 0);
		EXPECT_EQ(elfc_parse_dynsym_section(&elfc, 0, 0x30, 2, dynstr_off, dynsym, ALLOC_STD), 0);
		const u32 *name_off = tbl_get_cell(&dynsym->data.dynsym.tbl, 0, 0);
		EXPECT_NE(name_off, NULL);
		if (name_off != NULL) {
			u16 alpha_off = 6;
			log_set_quiet(0, 1);
			EXPECT_EQ(
				elfc_map_name(&dynsym->data.dynsym.tbl, 0, 99, &alpha_off, (void *)((byte *)elfc.bytes.data + dynstr_off)),
				1);
			log_set_quiet(0, 0);
		}
	}

	elfc_sect_t *reladyn = arr_add(&elfc.sects, NULL);
	EXPECT_NE(reladyn, NULL);
	if (reladyn != NULL) {
		mem_set(reladyn, 0, sizeof(*reladyn));
		reladyn->type = ELF_SECT_TYPE_RELADYN;

		byte reladyn_bytes[0x20] = {0};
		t_x86_put64(reladyn_bytes, 0x00, 0x401200);
		t_x86_put32(reladyn_bytes, 0x08, 6);
		t_x86_put32(reladyn_bytes, 0x0C, 1);
		t_x86_put64(reladyn_bytes, 0x10, 0);
		EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, reladyn_bytes, 0x18), 0);
		EXPECT_EQ(elfc_parse_reladyn_section(&elfc, 0, 0x18, 2, 1, reladyn, ALLOC_STD), 0);
		const u32 *bind = tbl_get_cell(&reladyn->data.reladyn.tbl, 0, 2);
		EXPECT_NE(bind, NULL);
		if (bind != NULL) {
			u32 zero_bind		     = 0;
			elfc_reladyn_name_map_t priv = {
				.elfc	   = &elfc,
				.dynsym_id = 1,
			};
			log_set_quiet(0, 1);
			EXPECT_EQ(elfc_map_reladyn_name(&reladyn->data.reladyn.tbl, 0, 99, &zero_bind, &priv), 1);
			log_set_quiet(0, 0);
		}
	}

	elfc_free(&elfc);

	END;
}

TEST(parse_x86_parse_note_branch_coverage)
{
	START;

	byte note_bytes[0x200] = {0};
	size_t note_len	       = t_x86_build_linux_note(note_bytes);
	const strv_t note_name = STRVT("note");

	elfc_t elfc = {0};
	elfc_init(&elfc, ALLOC_STD);
	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, note_bytes, note_len), 0);

	elfc_sect_t *sect = t_x86_add_sect(&elfc, ELF_SECT_TYPE_NOTE, 0, note_len, note_name);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		log_set_quiet(0, 1);
		EXPECT_EQ(elfc_parse_note_section(&elfc, 0, note_len, sect, ALLOC_STD), 0);
		log_set_quiet(0, 0);
		EXPECT_GT(sect->data.note.notes.cnt, 0);
	}

	asmc_t asmc = {0};
	asmc_init(&asmc, 64, ALLOC_STD);
	EXPECT_EQ(elfc_asmc(&elfc, &asmc), 0);
	EXPECT_GT(asmc.ops.cnt, 0);

	asmc_free(&asmc);
	elfc_free(&elfc);

	END;
}

TEST(parse_x86_parse_note_truncated)
{
	START;

	elfc_t elfc = {0};
	elfc_init(&elfc, ALLOC_STD);
	byte note_bytes[1]     = {0};
	const strv_t note_name = STRVT("note");
	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, note_bytes, sizeof(note_bytes)), 0);

	elfc_sect_t *sect = t_x86_add_sect(&elfc, ELF_SECT_TYPE_NOTE, 0, 1, note_name);
	EXPECT_NE(sect, NULL);
	if (sect != NULL) {
		EXPECT_NE(elfc_parse_note_section(&elfc, 0, 1, sect, ALLOC_STD), 0);
	}

	elfc_free(&elfc);

	END;
}

TEST(parse_x86_parse_program_branch_coverage)
{
	START;

	size_t ops_cnt = 0;

	const byte mods[] = {
		0x01, 0x00, 0x29, 0x00, 0x31, 0x00, 0x39, 0x00, 0x85, 0x00, 0x89, 0x00, 0x8B, 0x00, 0x8D, 0x00,
		0xC1, 0xC0, 0x00, 0xC6, 0xC0, 0x00, 0xC7, 0xC8, 0x00, 0xD1, 0xC0, 0x50, 0x54, 0x55, 0x5D, 0x5E,
	};
	EXPECT_EQ(t_x86_parse_program_blob(mods, sizeof(mods), 1, &ops_cnt), 0);
	EXPECT_GT(ops_cnt, 0);

	const byte data_errors[] = {
		0x80, 0x2D, 0x80, 0x3E, 0x80, 0x3D, 0x83, 0xC0, 0x00, 0x83, 0xE0, 0x00, 0x83, 0xE8, 0x00, 0xB8,
		0x78, 0x56, 0x34, 0x12, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00,
		0xC1, 0xE8, 0x00, 0xC1, 0xF8, 0x00, 0xC7, 0xC0, 0x11, 0x22, 0x33, 0x44, 0xFF, 0x15, 0x00, 0x00,
		0x00, 0x00, 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x35, 0x00, 0x00, 0x00, 0x00,
	};
	EXPECT_EQ(t_x86_parse_program_blob(data_errors, sizeof(data_errors), 2, &ops_cnt), 0);
	EXPECT_GT(ops_cnt, 0);

	const byte more_opcodes[] = {
		0x03, 0xC1, 0x09, 0xC1, 0x0B, 0xC1, 0x21, 0xC1, 0x23, 0xC1, 0x2B, 0xC1, 0x33, 0xC1, 0x3B, 0xC1, 0x68, 0x78, 0x56, 0x34,
		0x12, 0x6A, 0x7F, 0x81, 0xC0, 0x78, 0x56, 0x34, 0x12, 0x48, 0xB8, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	};
	EXPECT_EQ(t_x86_parse_program_blob(more_opcodes, sizeof(more_opcodes), 1, &ops_cnt), 0);
	EXPECT_GT(ops_cnt, 0);

	const byte prefix_ext[] = {
		0xF3, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0xF3, 0xFF, 0x35, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x40, 0x00, 0xFF, 0xC0,
		0x0F, 0x00, 0x0F, 0x1E, 0x00, 0x0F, 0x1F, 0x08, 0x0F, 0x1F, 0x40, 0x7F, 0x0F, 0x1F, 0x44, 0x24, 0x7F, 0x0F, 0x1F,
		0x80, 0x78, 0x56, 0x34, 0x12, 0x0F, 0x1F, 0x84, 0x24, 0x78, 0x56, 0x34, 0x12, 0x66, 0x48, 0x31, 0xC0, 0x00,
	};
	EXPECT_EQ(t_x86_parse_program_blob(prefix_ext, sizeof(prefix_ext), 1, &ops_cnt), 0);
	EXPECT_GT(ops_cnt, 0);

	END;
}

TEST(parse_x86_parse_prefix_group_coverage)
{
	START;

	const byte more_ops[] = {
		0x66, 0x05, 0x34, 0x12, 0x66, 0x0D, 0x78, 0x56, 0x66, 0x25, 0xBC, 0x9A, 0x66, 0x2D, 0x21, 0x43, 0x66, 0x35, 0x65, 0x87,
		0x66, 0x3D, 0xA9, 0xCB, 0x66, 0x8B, 0xC0, 0xF0, 0x67, 0x90, 0x0F, 0x84, 0x04, 0x00, 0x00, 0x00, 0x0F, 0x85, 0xFC, 0xFF,
		0xFF, 0xFF, 0x0F, 0x05, 0x0F, 0x1E, 0xFA, 0xFF, 0xC8, 0x83, 0xD0, 0x01, 0x83, 0xD8, 0x01, 0x83, 0xF0, 0x01,
	};

	size_t ops_cnt = 0;
	EXPECT_EQ(t_x86_parse_program_blob(more_ops, sizeof(more_ops), 1, &ops_cnt), 0);
	EXPECT_GT(ops_cnt, 0);

	END;
}

TEST(parse_x86_elfc_asmc_null)
{
	START;

	asmc_t asmc = {0};
	asmc_init(&asmc, 4, ALLOC_STD);

	EXPECT_EQ(elfc_asmc(NULL, NULL), 1);
	EXPECT_EQ(elfc_asmc(&((elfc_t){0}), NULL), 1);
	EXPECT_EQ(elfc_asmc(NULL, &asmc), 1);

	asmc_free(&asmc);

	END;
}

TEST(parse_x86_elfc_asmc_unknown_non_zero)
{
	START;

	elfc_t elfc = {0};
	EXPECT_EQ(elfc_init(&elfc, ALLOC_STD), &elfc);

	const byte bytes[] = {0x00, 0x7F, 0x00};
	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, bytes, sizeof(bytes)), 0);

	asmc_t asmc = {0};
	EXPECT_NE(asmc_init(&asmc, 16, ALLOC_STD), NULL);
	EXPECT_EQ(elfc_asmc(&elfc, &asmc), 0);
	EXPECT_GT(asmc.ops.cnt, 0);

	int found_non_zero = 0;
	const asmc_op_t *op = NULL;
	uint i		   = 0;
	arr_foreach(&asmc.ops, i, op)
	{
		if (op->type == ASMC_OP_BYTE && op->dst.val == 0x7F) {
			found_non_zero = 1;
			break;
		}
	}
	EXPECT_EQ(found_non_zero, 1);

	asmc_free(&asmc);
	elfc_free(&elfc);

	END;
}

TEST(parse_x86_arch_parse_reinit)
{
	START;

	reverse_image_t image = {0};
	EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);

	bin_t bin = {0};
	EXPECT_NE(bin_init(&bin, 4, ALLOC_STD), NULL);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, (byte[]){0x90, 0xC3, 0x00, 0x00}, 4), 0);
	EXPECT_EQ(reverse_image_set_bin(&image, &bin), 0);

	reverse_image_section_t desc = {
		.name	   = STRVT(".text"),
		.flags	   = REVERSE_IMAGE_SECTION_EXEC,
		.off	   = 0,
		.size	   = 2,
		.data	   = REVERSE_IMAGE_DATA_LE,
		.asmc_init = 1,
	};
	EXPECT_EQ(asmc_init(&desc.asmc, 1, ALLOC_STD), &desc.asmc);

	reverse_image_section_t *section = reverse_image_add_section(&image, &desc, NULL);
	EXPECT_NE(section, NULL);
	if (section != NULL) {
		asmc_op_t *op = asmc_add_op(&section->asmc, 0, ASMC_OP_NOP);
		EXPECT_NE(op, NULL);
	}

	arch_driver_t *arch = arch_driver_find(STRV("x86"));
	EXPECT_NE(arch, NULL);
	if (arch != NULL) {
		EXPECT_EQ(arch->parse(arch, NULL, ALLOC_STD), 1);
		EXPECT_EQ(arch->parse(arch, &image, ALLOC_STD), 0);
	}

	section = arr_get(&image.sections, 0);
	EXPECT_NE(section, NULL);
	if (section != NULL) {
		EXPECT_EQ(section->asmc_init, 1);
		EXPECT_GT(section->asmc.ops.cnt, 0);
	}

	reverse_image_free(&image);
	bin_free(&bin);

	END;
}

TEST(parse_x86_arch_parse_error)
{
	START;

	reverse_image_t image = {0};
	EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);

	bin_t bin = {0};
	EXPECT_NE(bin_init(&bin, 4, ALLOC_STD), NULL);
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, (byte[]){0x90, 0x90, 0x00, 0x00}, 4), 0);
	EXPECT_EQ(reverse_image_set_bin(&image, &bin), 0);

	reverse_image_section_t desc = {
		.name	   = STRVT(".text"),
		.flags	   = REVERSE_IMAGE_SECTION_EXEC,
		.off	   = 8,
		.size	   = 2,
		.data	   = REVERSE_IMAGE_DATA_LE,
		.asmc_init = 0,
	};
	reverse_image_section_t *section = reverse_image_add_section(&image, &desc, NULL);
	EXPECT_NE(section, NULL);

	arch_driver_t *arch = arch_driver_find(STRV("x86"));
	EXPECT_NE(arch, NULL);
	if (arch != NULL) {
		EXPECT_EQ(arch->parse(arch, &image, ALLOC_STD), 1);
	}

	reverse_image_free(&image);
	bin_free(&bin);

	END;
}

TEST(parse_x86_elfc_asmc_program_and_section_coverage)
{
	START;

	elfc_t elfc = {0};
	elfc_init(&elfc, ALLOC_STD);

	byte image[64] = {0};
	mem_copy(image + 32, 4, (byte[]){0x11, 0x22, 0x33, 0x44}, 4);
	EXPECT_EQ(t_x86_set_elfc_bytes(&elfc, image, sizeof(image)), 0);

	const strv_t text_name = STRVT("_text");
	elfc_sect_t *text_sect = t_x86_add_sect(&elfc, ELF_SECT_TYPE_SECTION, 0, 20, text_name);
	EXPECT_NE(text_sect, NULL);

	const strv_t prog_name = STRVT("program_blob");
	elfc_sect_t *prog_sect = t_x86_add_sect(&elfc, ELF_SECT_TYPE_PROGRAM, 32, 4, prog_name);
	EXPECT_NE(prog_sect, NULL);
	if (prog_sect != NULL) {
		asmc_init(&prog_sect->data.program.asmc, 16, ALLOC_STD);

		asmc_op_t *op = asmc_add_op(&prog_sect->data.program.asmc, 0, ASMC_OP_SECTION);
		strvbuf_add(&prog_sect->data.program.asmc.strs, STRV("section_name"), &op->str);
		op->str_off = 1;
		op->off	    = 7;

		op = asmc_add_op(&prog_sect->data.program.asmc, 0, ASMC_OP_GLOBAL);
		strvbuf_add(&prog_sect->data.program.asmc.strs, STRV("global_name"), &op->str);
		op->str_off = 1;

		op = asmc_add_op(&prog_sect->data.program.asmc, 0, ASMC_OP_LABEL);
		strvbuf_add(&prog_sect->data.program.asmc.strs, STRV("label_name"), &op->str);
		op->str_off = 1;

		op = asmc_add_op(&prog_sect->data.program.asmc, 0, ASMC_OP_STRING);
		strvbuf_add(&prog_sect->data.program.asmc.strs, STRV("string_name"), &op->str);
		op->str_off = 1;
		op->off	    = 3;
	}

	asmc_t asmc = {0};
	asmc_init(&asmc, 128, ALLOC_STD);
	EXPECT_EQ(elfc_asmc(&elfc, &asmc), 0);
	EXPECT_GT(asmc.ops.cnt, 0);

	asmc_free(&asmc);
	elfc_free(&elfc);

	END;
}

STEST(parse_x86)
{
	SSTART;

	RUN(parse_x86_elfc_free);
	RUN(parse_x86_elfc_asmc_manual);
	RUN(parse_x86_elfc_init_oom);
	RUN(parse_bin_x86_parse);
	RUN(parse_x86_parse_elf_layouts_32);
	RUN(parse_x86_parse_self);
	RUN(parse_x86_helpers_reg_lookup);
	RUN(parse_x86_helpers_reg_coverage);
	RUN(parse_x86_helpers_value_readers);
	RUN(parse_x86_helpers_signed_and_relative_readers);
	RUN(parse_x86_helpers_elfc_parse_errors);
	RUN(parse_x86_parse_notes_and_tables);
	RUN(parse_x86_parse_notes_linux_isa);
	RUN(parse_x86_parse_program_section_bounds);
	RUN(parse_x86_parse_program_section_init_oom);
	RUN(parse_x86_parse_program_section_decode_oom);
	RUN(parse_x86_parse_program_section);
	RUN(parse_x86_parse_program_section_address_truncation);
	RUN(parse_x86_parse_program_section_opcode_truncation);
	RUN(parse_x86_parse_program_section_control_truncation);
	RUN(parse_x86_parse_decode_edge_coverage);
	RUN(parse_x86_parse_add_op_failure_coverage);
	RUN(parse_x86_parse_full_image);
	RUN(parse_x86_parse_full_image_unknown_machine_data);
	RUN(parse_x86_parse_full_image_big_endian_note_unknowns);
	RUN(parse_x86_parse_full_image_add_image_section_failure);
	RUN(parse_bin_x86_null);
	RUN(parse_x86_asmc_schema);
	RUN(parse_x86_map_name_oom);
	RUN(parse_x86_parse_note_branch_coverage);
	RUN(parse_x86_parse_note_truncated);
	RUN(parse_x86_parse_program_branch_coverage);
	RUN(parse_x86_parse_prefix_group_coverage);
	RUN(parse_x86_elfc_asmc_null);
	RUN(parse_x86_elfc_asmc_unknown_non_zero);
	RUN(parse_x86_arch_parse_reinit);
	RUN(parse_x86_arch_parse_error);
	RUN(parse_x86_elfc_asmc_program_and_section_coverage);

	SEND;
}
