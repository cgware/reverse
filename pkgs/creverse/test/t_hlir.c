#include "hlir.h"

#include <limits.h>

#include "log.h"
#include "mem.h"
#include "str.h"
#include "test.h"

static int t_c_str_contains(strv_t haystack, strv_t needle)
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

static int t_c_str_eq(const char *a, const char *b)
{
	if (a == NULL || b == NULL) {
		return a == b;
	}

	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return 0;
		}
		a++;
		b++;
	}

	return *a == '\0' && *b == '\0';
}

TEST(hlir_kind_name_all)
{
	START;

	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_ROOT), "root"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_INCLUDE), "include"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_FUNCTION), "function"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_BLOCK), "block"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_DECL), "decl"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_LABEL), "label"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_PHI), "phi"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_ASSIGN), "assign"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_BIN_ASSIGN), "bin_assign"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_IF_GOTO), "if_goto"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_IF), "if"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_WHILE), "while"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_GOTO), "goto"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_SWAP), "swap"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_CALL), "call"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_RETURN), "return"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_STMT_UNKNOWN), "unknown"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_EXPR_CONST), "const"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_EXPR_REF), "ref"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_EXPR_UNARY), "unary"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_EXPR_BINARY), "binary"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name(HLIR_KIND_EXPR_UNKNOWN), "expr_unknown"), 1);
	EXPECT_EQ(t_c_str_eq(hlir_kind_name((hlir_kind_t)-1), "unknown"), 1);

	END;
}

TEST(hlir_api_null_safety)
{
	START;

	hlir_t hlir = {0};
	hlir_t bad = {0};
	uint node = 0;

	EXPECT_EQ(hlir_init(NULL), NULL);
	EXPECT_EQ(hlir_new(NULL, NULL, HLIR_KIND_ROOT, STRV_NULL, 0), NULL);
	EXPECT_EQ(hlir_set_kind(NULL, 0, HLIR_KIND_ROOT), 1);
	EXPECT_EQ(hlir_set_text(NULL, 0, STRV("x")), 1);
	EXPECT_EQ(hlir_setf(NULL, 0, "%s", "x"), 1);
	EXPECT_EQ(hlir_setf(&hlir, 0, NULL), 1);
	EXPECT_EQ(hlir_remove(NULL, 0), 1);
	EXPECT_EQ(hlir_get(NULL, 0), NULL);
	EXPECT_EQ(hlir_print(NULL, DST_NONE()), 0);
	hlir_t zero = {0};
	EXPECT_EQ(hlir_print(&zero, DST_NONE()), 0);
	hlir_free(NULL);
	hlir_reset(NULL);

	mem_oom(1);
	EXPECT_EQ(hlir_init(&bad), NULL);
	mem_oom(0);

	mem_oom(2);
	EXPECT_EQ(hlir_init(&bad), NULL);
	mem_oom(0);

	EXPECT_NE(hlir_init(&hlir), NULL);
	EXPECT_NE(hlir_get(&hlir, HLIR_ROOT), NULL);
	EXPECT_EQ(hlir_get(&hlir, HLIR_ROOT)->kind, HLIR_KIND_ROOT);
	EXPECT_EQ(hlir_get(&hlir, HLIR_ROOT)->depth, 0);

	EXPECT_NE(hlir_new(&hlir, &node, HLIR_KIND_INCLUDE, STRV("x"), 1), NULL);
	EXPECT_NE(node, 0);
	EXPECT_EQ(hlir_remove(&hlir, node), 0);
	EXPECT_EQ(hlir_set_kind(&hlir, node, HLIR_KIND_INCLUDE), 1);
	EXPECT_EQ(hlir_set_text(&hlir, node, STRV("x")), 1);
	EXPECT_EQ(hlir_remove(&hlir, node), 1);
	EXPECT_EQ(hlir_get(&hlir, node), NULL);

	mem_oom(1);
	EXPECT_EQ(hlir_new(&hlir, &node, HLIR_KIND_INCLUDE, STRV("x"), 1), NULL);
	mem_oom(0);

	mem_oom(2);
	EXPECT_EQ(hlir_new(&hlir, &node, HLIR_KIND_INCLUDE, STRV("x"), 1), NULL);
	mem_oom(0);

	mem_oom(1);
	EXPECT_EQ(hlir_set_text(&hlir, HLIR_ROOT, STRV("x")), 1);
	mem_oom(0);

	mem_oom(1);
	EXPECT_EQ(hlir_setf(&hlir, HLIR_ROOT, "%s", "x"), 1);
	mem_oom(0);

	hlir_free(&hlir);

	END;
}

TEST(hlir_construct_array)
{
	START;

	hlir_t hlir = {0};
	EXPECT_NE(hlir_init(&hlir), NULL);

	uint include = 0;
	uint func = 0;
	uint body = 0;
	uint decl = 0;
	uint label = 0;
	uint expr = 0;

	EXPECT_NE(hlir_new(&hlir, &include, HLIR_KIND_INCLUDE, STRV("stdbool.h"), 1), NULL);
	EXPECT_NE(hlir_new(&hlir, &func, HLIR_KIND_FUNCTION, STRV("recovered"), 1), NULL);
	EXPECT_NE(hlir_new(&hlir, &body, HLIR_KIND_BLOCK, STRV_NULL, 2), NULL);
	EXPECT_NE(hlir_new(&hlir, &decl, HLIR_KIND_DECL, STRV("uint8_t A"), 3), NULL);
	EXPECT_NE(hlir_new(&hlir, &label, HLIR_KIND_LABEL, STRV("block1"), 3), NULL);
	EXPECT_NE(hlir_new(&hlir, &expr, HLIR_KIND_EXPR_CONST, STRV("0x01"), 3), NULL);

	EXPECT_EQ(include, 1);
	EXPECT_EQ(func, 2);
	EXPECT_EQ(body, 3);
	EXPECT_EQ(decl, 4);
	EXPECT_EQ(label, 5);
	EXPECT_EQ(expr, 6);

	EXPECT_EQ(hlir_get(&hlir, include)->kind, HLIR_KIND_INCLUDE);
	EXPECT_EQ(hlir_get(&hlir, func)->kind, HLIR_KIND_FUNCTION);
	EXPECT_EQ(hlir_get(&hlir, body)->kind, HLIR_KIND_BLOCK);
	EXPECT_EQ(hlir_get(&hlir, decl)->kind, HLIR_KIND_DECL);

	char out[256] = {0};
	size_t len = hlir_print(&hlir, DST_BUF(out));
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("root")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("include stdbool.h")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("function recovered")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("block")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("decl uint8_t A")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("label block1")), 1);

	EXPECT_EQ(hlir_remove(&hlir, label), 0);
	EXPECT_EQ(hlir_get(&hlir, label), NULL);

	hlir_reset(&hlir);
	EXPECT_NE(hlir_get(&hlir, HLIR_ROOT), NULL);
	EXPECT_EQ(hlir_get(&hlir, HLIR_ROOT)->kind, HLIR_KIND_ROOT);
	EXPECT_EQ(hlir_get(&hlir, HLIR_ROOT)->depth, 0);
	log_set_quiet(0, 1);
	EXPECT_EQ(hlir_get(&hlir, include), NULL);
	log_set_quiet(0, 0);

	hlir_free(&hlir);

	END;
}

TEST(hlir_set_text_and_print)
{
	START;

	hlir_t hlir = {0};
	EXPECT_NE(hlir_init(&hlir), NULL);

	uint node = 0;
	EXPECT_NE(hlir_new(&hlir, &node, HLIR_KIND_INCLUDE, STRV("stdbool.h"), 1), NULL);
	EXPECT_EQ(hlir_set_kind(&hlir, node, HLIR_KIND_INCLUDE), 0);
	EXPECT_EQ(hlir_set_text(&hlir, node, STRV("stdint.h")), 0);
	EXPECT_EQ(hlir_setf(&hlir, node, "%s", "stdbool.h"), 0);

	char out[128] = {0};
	size_t len = hlir_print(&hlir, DST_BUF(out));
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("include stdbool.h")), 1);

	hlir_free(&hlir);

	END;
}

STEST(hlir)
{
	SSTART;

	RUN(hlir_api_null_safety);
	RUN(hlir_construct_array);
	RUN(hlir_set_text_and_print);
	RUN(hlir_kind_name_all);

	SEND;
}
