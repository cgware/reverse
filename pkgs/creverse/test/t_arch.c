#include "arch.h"

#include "test.h"

static int t_arch_str_contains(strv_t haystack, strv_t needle)
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

TEST(arch_find)
{
	START;

	arch_driver_t *parse_8051 = arch_driver_find(STRV("8051"));
	EXPECT_NE(parse_8051, NULL);
	if (parse_8051 != NULL) {
		EXPECT_EQ(parse_8051->name.len, 4);
		EXPECT_EQ(strv_eq(parse_8051->name, STRV("8051")), 1);
		EXPECT_NE(parse_8051->parse, NULL);
	}

	arch_driver_t *parse_x86 = arch_driver_find(STRV("x86"));
	EXPECT_NE(parse_x86, NULL);
	if (parse_x86 != NULL) {
		EXPECT_EQ(parse_x86->name.len, 3);
		EXPECT_EQ(strv_eq(parse_x86->name, STRV("x86")), 1);
		EXPECT_NE(parse_x86->parse, NULL);
	}

	EXPECT_EQ(arch_driver_find(STRV("missing")), NULL);
	EXPECT_EQ(arch_driver_find(STRV_NULL), NULL);

	END;
}

TEST(arch_detect)
{
	START;

	reverse_image_t image = {0};
	EXPECT_NE(reverse_image_init(&image, ALLOC_STD), NULL);

	image.machine	    = REVERSE_IMAGE_MACHINE_X86;
	arch_driver_t *arch = arch_driver_detect(&image);
	EXPECT_NE(arch, NULL);
	if (arch != NULL) {
		EXPECT_EQ(strv_eq(arch->name, STRV("x86")), 1);
	}

	image.machine = REVERSE_IMAGE_MACHINE_8051;
	arch	      = arch_driver_detect(&image);
	EXPECT_NE(arch, NULL);
	if (arch != NULL) {
		EXPECT_EQ(strv_eq(arch->name, STRV("8051")), 1);
	}

	image.machine = REVERSE_IMAGE_MACHINE_UNKNOWN;
	arch	      = arch_driver_detect(&image);
	EXPECT_NE(arch, NULL);
	if (arch != NULL) {
		EXPECT_EQ(strv_eq(arch->name, STRV("8051")), 1);
	}

	EXPECT_EQ(arch_driver_detect(NULL), NULL);

	reverse_image_free(&image);

	END;
}

TEST(arch_print)
{
	START;

	char buf[256] = {0};
	EXPECT_GT(arch_drivers_print(DST_BUF(buf)), 0);
	EXPECT_NE(t_arch_str_contains(strv_cstr(buf), STRV("8051")), 0);
	EXPECT_NE(t_arch_str_contains(strv_cstr(buf), STRV("x86")), 0);

	END;
}

STEST(arch)
{
	SSTART;

	RUN(arch_find);
	RUN(arch_detect);
	RUN(arch_print);

	SEND;
}
