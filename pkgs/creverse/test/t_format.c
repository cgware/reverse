#include "format.h"

#include "mem.h"
#include "t_drivers.h"
#include "test.h"

static int t_format_str_contains(strv_t haystack, strv_t needle)
{
	if (needle.len == 0) {
		return 1;
	}

	if (haystack.len < needle.len) {
		return 0;
	}

	for (size_t i = 0; i + needle.len <= haystack.len; i++) {
		if (strv_eq(STRVN(&haystack.data[i], needle.len), needle)) {
			return 1;
		}
	}

	return 0;
}

TEST(format_find)
{
	START;

	format_driver_t *format = format_driver_find(STRV("elf"));
	EXPECT_NE(format, NULL);
	if (format != NULL) {
		EXPECT_EQ(strv_eq(format->name, STRV("elf")), 1);
		EXPECT_NE(format->parse, NULL);
	}

	format = format_driver_find(STRV("bin"));
	EXPECT_NE(format, NULL);
	if (format != NULL) {
		EXPECT_EQ(strv_eq(format->name, STRV("bin")), 1);
		EXPECT_NE(format->parse, NULL);
		EXPECT_NE(format->emit, NULL);
	}

	format = format_driver_find(STRV("rtl8373n"));
	EXPECT_NE(format, NULL);
	if (format != NULL) {
		EXPECT_EQ(strv_eq(format->name, STRV("rtl8373n")), 1);
		EXPECT_NE(format->parse, NULL);
		EXPECT_NE(format->emit, NULL);
	}

	EXPECT_EQ(format_driver_find(STRV("missing")), NULL);
	EXPECT_EQ(format_driver_find(STRV_NULL), NULL);

	END;
}

TEST(format_detect)
{
	START;

	bin_t bin = {0};
	EXPECT_NE(bin_init(&bin, 0x2F9E0, ALLOC_STD), NULL);

	u8 elf_magic[] = {0x7F, 'E', 'L', 'F'};
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, elf_magic, sizeof(elf_magic)), 0);
	format_driver_t *format = format_driver_detect(&bin);
	EXPECT_NE(format, NULL);
	if (format != NULL) {
		EXPECT_EQ(strv_eq(format->name, STRV("elf")), 1);
	}

	EXPECT_EQ(bin_resize(&bin, 0x2F9E0), 0);
	mem_set(bin.buf.data, 0, 0x2F9E0);
	bin.buf.used		       = 0x2F9E0;
	((byte *)bin.buf.data)[0x0002] = 0x02;
	((byte *)bin.buf.data)[0x4000] = 0x90;
	((byte *)bin.buf.data)[0x1000] = 0x01;
	format			       = format_driver_detect(&bin);
	EXPECT_NE(format, NULL);
	if (format != NULL) {
		EXPECT_EQ(strv_eq(format->name, STRV("rtl8373n")), 1);
	}

	u8 raw[] = {0x01, 0x02, 0x03};
	EXPECT_EQ(t_drivers_bin_from_bytes(&bin, raw, sizeof(raw)), 0);
	format = format_driver_detect(&bin);
	EXPECT_NE(format, NULL);
	if (format != NULL) {
		EXPECT_EQ(strv_eq(format->name, STRV("bin")), 1);
	}

	EXPECT_EQ(format_driver_detect(NULL), NULL);

	bin_free(&bin);

	END;
}

TEST(format_print)
{
	START;

	char buf[256] = {0};
	EXPECT_GT(format_drivers_print(DST_BUF(buf)), 0);
	EXPECT_NE(t_format_str_contains(strv_cstr(buf), STRV("bin")), 0);
	EXPECT_NE(t_format_str_contains(strv_cstr(buf), STRV("elf")), 0);
	EXPECT_NE(t_format_str_contains(strv_cstr(buf), STRV("rtl8373n")), 0);

	END;
}

TEST(format_emit_image_sections)
{
	START;

	EXPECT_EQ(format_emit_image_sections(NULL, &(asmc_t){0}), 1);
	EXPECT_EQ(format_emit_image_sections(&(reverse_image_t){0}, NULL), 1);

	reverse_image_t image = {0};
	EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);

	reverse_image_section_t empty = {0};
	EXPECT_NE(reverse_image_add_section(&image, &empty, NULL), NULL);

	reverse_image_section_t section = {
		.asmc_init = 1,
	};
	EXPECT_EQ(asmc_init(&section.asmc, 4, ALLOC_STD), &section.asmc);
	size_t sec = 0;
	size_t lbl = 0;
	strvbuf_add(&section.asmc.strs, STRV(".text"), &sec);
	strvbuf_add(&section.asmc.strs, STRV("label"), &lbl);

	asmc_op_t *op = asmc_add_op(&section.asmc, 0, ASMC_OP_SECTION);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->str = sec;
	}
	op = asmc_add_op(&section.asmc, 0x10, ASMC_OP_LABEL);
	EXPECT_NE(op, NULL);
	if (op != NULL) {
		op->str = lbl;
	}
	op = asmc_add_op(&section.asmc, 0x11, ASMC_OP_RET);
	EXPECT_NE(op, NULL);

	EXPECT_NE(reverse_image_add_section(&image, &section, NULL), NULL);

	asmc_t out = {0};
	EXPECT_EQ(asmc_init(&out, 4, ALLOC_STD), &out);
	EXPECT_EQ(format_emit_image_sections(&image, &out), 0);
	EXPECT_EQ(out.ops.cnt, 3);
	EXPECT_GT(out.strs.used, 0);

	asmc_t out_fail = {0};
	EXPECT_EQ(asmc_init(&out_fail, 8, ALLOC_STD), &out_fail);
	for (uint i = 0; i < 16; i++) {
		size_t off = 0;
		EXPECT_EQ(strvbuf_add(&out_fail.strs, STRV("fill_fill_fill_"), &off), 0);
	}
	mem_oom(1);
	EXPECT_EQ(format_emit_image_sections(&image, &out_fail), 1);
	mem_oom(0);

	asmc_t out_ops_fail = {0};
	EXPECT_EQ(asmc_init(&out_ops_fail, 1, ALLOC_STD), &out_ops_fail);
	EXPECT_NE(asmc_add_op(&out_ops_fail, 0, ASMC_OP_NOP), NULL);
	mem_oom(1);
	EXPECT_EQ(format_emit_image_sections(&image, &out_ops_fail), 1);
	mem_oom(0);

	asmc_free(&out_ops_fail);
	asmc_free(&out_fail);
	asmc_free(&out);
	reverse_image_free(&image);

	END;
}

STEST(format)
{
	SSTART;

	RUN(format_find);
	RUN(format_detect);
	RUN(format_print);
	RUN(format_emit_image_sections);

	SEND;
}
