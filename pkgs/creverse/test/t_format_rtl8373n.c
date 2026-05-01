#include "format.h"

#include "mem.h"
#include "t_drivers.h"
#include "test.h"

enum {
	T_RTL8373N_IMAGE_SIZE = 0x2F9E0,
};

static int t_format_rtl8373n_init_bin(bin_t *bin)
{
	if (bin_init(bin, T_RTL8373N_IMAGE_SIZE, ALLOC_STD) == NULL) {
		return 1;
	}

	if (bin_resize(bin, T_RTL8373N_IMAGE_SIZE)) {
		return 1;
	}

	mem_set(bin->buf.data, 0, T_RTL8373N_IMAGE_SIZE);
	bin->buf.used			= T_RTL8373N_IMAGE_SIZE;
	((byte *)bin->buf.data)[0x0002] = 0x02;
	((byte *)bin->buf.data)[0x4000] = 0x90;
	((byte *)bin->buf.data)[0x1000] = 0x01;
	return 0;
}

TEST(format_rtl8373n_probe)
{
	START;

	EXPECT_EQ(format_driver_detect(NULL), NULL);

	bin_t bin = {0};
	EXPECT_EQ(t_format_rtl8373n_init_bin(&bin), 0);
	format_driver_t *format = format_driver_detect(&bin);
	EXPECT_NE(format, NULL);
	if (format != NULL) {
		EXPECT_EQ(strv_eq(format->name, STRV("rtl8373n")), 1);
	}

	((byte *)bin.buf.data)[0x1000] = 0x00;
	format			       = format_driver_detect(&bin);
	EXPECT_NE(format, NULL);
	if (format != NULL) {
		EXPECT_EQ(strv_eq(format->name, STRV("bin")), 1);
	}

	((byte *)bin.buf.data)[0x1000] = 0x01;
	bin.buf.used		       = T_RTL8373N_IMAGE_SIZE - 1;
	format			       = format_driver_detect(&bin);
	EXPECT_NE(format, NULL);
	if (format != NULL) {
		EXPECT_EQ(strv_eq(format->name, STRV("bin")), 1);
	}

	bin.buf.used = T_RTL8373N_IMAGE_SIZE;
	void *data   = bin.buf.data;
	bin.buf.data = NULL;
	format	     = format_driver_detect(&bin);
	EXPECT_NE(format, NULL);
	if (format != NULL) {
		EXPECT_EQ(strv_eq(format->name, STRV("bin")), 1);
	}
	bin.buf.data = data;

	bin_free(&bin);

	END;
}

TEST(format_rtl8373n_parse)
{
	START;

	format_driver_t *format = format_driver_find(STRV("rtl8373n"));
	EXPECT_NE(format, NULL);

	bin_t bin = {0};
	EXPECT_EQ(t_format_rtl8373n_init_bin(&bin), 0);

	reverse_image_t image = {0};
	EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);
	if (format != NULL) {
		EXPECT_NE(format->parse(format, NULL, &image, DST_NONE(), ALLOC_STD), 0);
		EXPECT_NE(format->parse(format, &bin, NULL, DST_NONE(), ALLOC_STD), 0);

		bin.buf.used = T_RTL8373N_IMAGE_SIZE - 1;
		EXPECT_NE(format->parse(format, &bin, &image, DST_NONE(), ALLOC_STD), 0);
		bin.buf.used = T_RTL8373N_IMAGE_SIZE;

		EXPECT_EQ(format->parse(format, &bin, &image, DST_NONE(), ALLOC_STD), 0);
		EXPECT_EQ(image.machine, REVERSE_IMAGE_MACHINE_8051);
		EXPECT_EQ(image.sections.cnt, 5);

		reverse_image_section_t *section = arr_get(&image.sections, 0);
		EXPECT_NE(section, NULL);
		if (section != NULL) {
			EXPECT_EQ(strv_eq(section->name, STRV("code0")), 1);
			EXPECT_EQ(section->off, 0);
			EXPECT_EQ(section->size, 0x3AB8);
			EXPECT_EQ(section->data, REVERSE_IMAGE_DATA_BE);
			EXPECT_EQ(section->flags & REVERSE_IMAGE_SECTION_EXEC, REVERSE_IMAGE_SECTION_EXEC);
		}

		section = arr_get(&image.sections, 4);
		EXPECT_NE(section, NULL);
		if (section != NULL) {
			EXPECT_EQ(strv_eq(section->name, STRV("patchdb")), 1);
			EXPECT_EQ(section->off, 0x28000);
			EXPECT_EQ(section->size, 0x79E0);
			EXPECT_EQ(section->flags & REVERSE_IMAGE_SECTION_EXEC, 0);
		}

		for (uint i = image.sections.cnt; i < 8; i++) {
			reverse_image_section_t filler = {0};
			EXPECT_NE(reverse_image_add_section(&image, &filler, NULL), NULL);
		}
		mem_oom(1);
		EXPECT_NE(format->parse(format, &bin, &image, DST_NONE(), ALLOC_STD), 0);
		mem_oom(0);
	}

	reverse_image_free(&image);
	bin_free(&bin);

	END;
}

TEST(format_rtl8373n_emit)
{
	START;

	format_driver_t *format = format_driver_find(STRV("rtl8373n"));
	EXPECT_NE(format, NULL);

	if (format != NULL) {
		EXPECT_EQ(format->emit(format, NULL, &(asmc_t){0}, ALLOC_STD), 1);
		EXPECT_EQ(format->emit(format, &(reverse_image_t){0}, NULL, ALLOC_STD), 1);

		reverse_image_t image = {0};
		EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);
		asmc_t asmc = {0};
		EXPECT_EQ(asmc_init(&asmc, 1, ALLOC_STD), &asmc);
		EXPECT_EQ(format->emit(format, &image, &asmc, ALLOC_STD), 0);
		asmc_free(&asmc);
		reverse_image_free(&image);
	}

	END;
}

STEST(format_rtl8373n)
{
	SSTART;

	RUN(format_rtl8373n_probe);
	RUN(format_rtl8373n_parse);
	RUN(format_rtl8373n_emit);

	SEND;
}
