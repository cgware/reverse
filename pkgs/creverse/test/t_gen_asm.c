#include "gen_asm.h"

#include "test.h"

static int t_gen_asm_str_contains(strv_t haystack, strv_t needle)
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

TEST(gen_asm_find)
{
	START;

	gen_asm_driver_t *asm_8051 = gen_asm_driver_find(STRV("8051"));
	EXPECT_NE(asm_8051, NULL);
	if (asm_8051 != NULL) {
		EXPECT_EQ(strv_eq(asm_8051->name, STRV("8051")), 1);
		EXPECT_NE(asm_8051->print, NULL);
	}

	gen_asm_driver_t *asm_x86 = gen_asm_driver_find(STRV("x86"));
	EXPECT_NE(asm_x86, NULL);
	if (asm_x86 != NULL) {
		EXPECT_EQ(strv_eq(asm_x86->name, STRV("x86")), 1);
		EXPECT_NE(asm_x86->print, NULL);
	}

	EXPECT_EQ(gen_asm_driver_find(STRV("missing")), NULL);

	END;
}

TEST(gen_asm_print)
{
	START;

	char buf[256] = {0};
	EXPECT_GT(gen_asm_drivers_print(DST_BUF(buf)), 0);
	EXPECT_NE(t_gen_asm_str_contains(strv_cstr(buf), STRV("8051")), 0);
	EXPECT_NE(t_gen_asm_str_contains(strv_cstr(buf), STRV("x86")), 0);

	END;
}

STEST(gen_asm)
{
	SSTART;

	RUN(gen_asm_find);
	RUN(gen_asm_print);

	SEND;
}
