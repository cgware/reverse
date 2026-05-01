#include "image.h"

#include "mem.h"
#include "test.h"

static int t_image_str_contains(strv_t haystack, strv_t needle)
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

TEST(image_init_free)
{
	START;

	EXPECT_EQ(reverse_image_init(NULL, ALLOC_STD), NULL);
	reverse_image_free(NULL);

	reverse_image_t image = {0};
	EXPECT_EQ(reverse_image_init(&image, ALLOC_STD), &image);
	EXPECT_NE(image.bin.buf.data, NULL);
	EXPECT_NE(image.sections.data, NULL);
	EXPECT_EQ(image.sections.cnt, 0);
	EXPECT_EQ(image.machine, REVERSE_IMAGE_MACHINE_UNKNOWN);
	EXPECT_EQ(image.priv, NULL);

	reverse_image_free(&image);

	mem_oom(1);
	EXPECT_EQ(reverse_image_init(&image, ALLOC_STD), NULL);
	mem_oom(0);

	END;
}

TEST(image_set_bin)
{
	START;

	bin_t bin = {0};
	EXPECT_NE(bin_init(&bin, 4, ALLOC_STD), NULL);
	((byte *)bin.buf.data)[0] = 0x11;
	((byte *)bin.buf.data)[1] = 0x22;
	((byte *)bin.buf.data)[2] = 0x33;
	((byte *)bin.buf.data)[3] = 0x44;
	bin.buf.used		  = 4;

	reverse_image_t image = {0};
	EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);

	EXPECT_EQ(reverse_image_set_bin(NULL, &bin), 1);
	EXPECT_EQ(reverse_image_set_bin(&image, NULL), 1);
	EXPECT_EQ(reverse_image_set_bin(&image, &bin), 0);
	EXPECT_EQ(image.bin.buf.used, 4);
	EXPECT_EQ(((byte *)image.bin.buf.data)[0], 0x11);
	EXPECT_EQ(((byte *)image.bin.buf.data)[3], 0x44);

	((byte *)bin.buf.data)[0] = 0xAA;
	EXPECT_EQ(((byte *)image.bin.buf.data)[0], 0x11);

	bin_t large = {0};
	EXPECT_NE(bin_init(&large, 28401, ALLOC_STD), NULL);
	large.buf.used = 28401;

	mem_oom(1);
	EXPECT_EQ(reverse_image_set_bin(&image, &large), 1);
	mem_oom(0);

	bin_free(&large);
	reverse_image_free(&image);
	bin_free(&bin);

	END;
}

TEST(image_add_section)
{
	START;

	reverse_image_t image = {0};
	EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);

	EXPECT_EQ(reverse_image_add_section(NULL, &(reverse_image_section_t){0}, NULL), NULL);
	EXPECT_EQ(reverse_image_add_section(&image, NULL, NULL), NULL);

	reverse_image_section_t desc = {
		.name	      = STRVT(".text"),
		.index	      = 3,
		.addr	      = 0x1000,
		.off	      = 0x20,
		.size	      = 0x40,
		.type	      = 1,
		.format_flags = 6,
		.align	      = 16,
		.entry_size   = 4,
		.link	      = 2,
		.info	      = 7,
		.data	      = REVERSE_IMAGE_DATA_LE,
		.flags	      = REVERSE_IMAGE_SECTION_EXEC,
		.priv	      = &image,
	};
	uint id				 = 99;
	reverse_image_section_t *section = reverse_image_add_section(&image, &desc, &id);
	EXPECT_NE(section, NULL);
	EXPECT_EQ(id, 0);
	if (section != NULL) {
		EXPECT_EQ(section->index, 3);
		EXPECT_EQ(section->addr, 0x1000);
		EXPECT_EQ(section->off, 0x20);
		EXPECT_EQ(section->size, 0x40);
		EXPECT_EQ(section->type, 1);
		EXPECT_EQ(section->format_flags, 6);
		EXPECT_EQ(section->align, 16);
		EXPECT_EQ(section->entry_size, 4);
		EXPECT_EQ(section->link, 2);
		EXPECT_EQ(section->info, 7);
		EXPECT_EQ(section->data, REVERSE_IMAGE_DATA_LE);
		EXPECT_EQ(section->flags, REVERSE_IMAGE_SECTION_EXEC);
		EXPECT_EQ(section->priv, &image);
	}

	reverse_image_section_t owned = {
		.name	   = STRVT(".owned"),
		.asmc_init = 1,
	};
	EXPECT_EQ(asmc_init(&owned.asmc, 1, ALLOC_STD), &owned.asmc);
	EXPECT_NE(reverse_image_add_section(&image, &owned, NULL), NULL);

	reverse_image_free(&image);

	END;
}

TEST(image_add_section_oom)
{
	START;

	reverse_image_t image = {0};
	EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);

	for (uint i = 0; i < 8; i++) {
		reverse_image_section_t section = {
			.index = i,
		};
		EXPECT_NE(reverse_image_add_section(&image, &section, NULL), NULL);
	}

	mem_oom(1);
	EXPECT_EQ(reverse_image_add_section(&image, &(reverse_image_section_t){.index = 8}, NULL), NULL);
	mem_oom(0);

	reverse_image_free(&image);

	END;
}

TEST(image_print_sections)
{
	START;

	EXPECT_EQ(reverse_image_print_sections(NULL, DST_NONE()), 0);

	reverse_image_t image = {0};
	EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);

	reverse_image_section_t text = {
		.name	      = STRVT(".text"),
		.index	      = 1,
		.addr	      = 0x401000,
		.off	      = 0x1000,
		.size	      = 0x20,
		.type	      = 1,
		.format_flags = 0x6,
		.align	      = 16,
		.entry_size   = 0,
		.link	      = 2,
		.info	      = 3,
		.data	      = REVERSE_IMAGE_DATA_LE,
		.flags	      = REVERSE_IMAGE_SECTION_WRITE | REVERSE_IMAGE_SECTION_ALLOC | REVERSE_IMAGE_SECTION_EXEC | 0x80,
	};
	reverse_image_section_t rodata = {
		.name = STRVT(".rodata"),
		.data = REVERSE_IMAGE_DATA_BE,
	};
	reverse_image_section_t unknown = {
		.name = STRVT(".unknown"),
		.data = 99,
	};
	EXPECT_NE(reverse_image_add_section(&image, &text, NULL), NULL);
	EXPECT_NE(reverse_image_add_section(&image, &rodata, NULL), NULL);
	EXPECT_NE(reverse_image_add_section(&image, &unknown, NULL), NULL);

	char out[2048] = {0};
	EXPECT_GT(reverse_image_print_sections(&image, DST_BUF(out)), 0);
	strv_t printed = strv_cstr(out);
	EXPECT_NE(t_image_str_contains(printed, STRV("idx  name")), 0);
	EXPECT_NE(t_image_str_contains(printed, STRV(".text")), 0);
	EXPECT_NE(t_image_str_contains(printed, STRV("0x0000000000401000")), 0);
	EXPECT_NE(t_image_str_contains(printed, STRV("write,alloc,exec,0x00000080")), 0);
	EXPECT_NE(t_image_str_contains(printed, STRV(".rodata")), 0);
	EXPECT_NE(t_image_str_contains(printed, STRV("be")), 0);
	EXPECT_NE(t_image_str_contains(printed, STRV(".unknown")), 0);
	EXPECT_NE(t_image_str_contains(printed, STRV("unknown")), 0);
	EXPECT_NE(t_image_str_contains(printed, STRV("-")), 0);

#ifdef C_LINUX
	char small[1] = {0};
	EXPECT_GT(reverse_image_print_sections(&image, DST_BUF(small)), 0);
#endif
	reverse_image_free(&image);

	END;
}

STEST(image)
{
	SSTART;

	RUN(image_init_free);
	RUN(image_set_bin);
	RUN(image_add_section);
	RUN(image_add_section_oom);
	RUN(image_print_sections);

	SEND;
}
