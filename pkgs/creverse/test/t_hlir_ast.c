#include "hlir_ast.h"
#include "ast_c.h"

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

static uint t_add(hlir_t *hlir, hlir_kind_t kind, strv_t text, uint depth)
{
	uint node = 0;
	if (hlir_new(hlir, &node, kind, text, depth) == NULL) {
		return UINT_MAX;
	}
	return node;
}

TEST(hlir_ast_null_safety)
{
	START;

	ast_t ast = {0};
	hlir_t hlir = {0};

	EXPECT_EQ(hlir_ast_gen(NULL, NULL), 1);
	EXPECT_EQ(hlir_ast_gen(&ast, NULL), 1);
	EXPECT_EQ(hlir_ast_gen(NULL, &hlir), 1);

	EXPECT_NE(ast_init(&ast), NULL);
	EXPECT_EQ(hlir_ast_gen(&ast, &hlir), 0);
	ast_free(&ast);

	END;
}

TEST(hlir_ast_clone_and_print)
{
	START;

	hlir_t hlir = {0};
	ast_t ast = {0};
	EXPECT_NE(hlir_init(&hlir), NULL);
	EXPECT_NE(ast_init(&ast), NULL);

	uint include = t_add(&hlir, HLIR_KIND_INCLUDE, STRV("stdbool.h"), 1);
	uint func = t_add(&hlir, HLIR_KIND_FUNCTION, STRV("recovered"), 1);
	uint body = t_add(&hlir, HLIR_KIND_BLOCK, STRV_NULL, 2);
	uint call = t_add(&hlir, HLIR_KIND_STMT_CALL, STRV("store_u16_le(xram, DPTR, 0x0005)"), 3);
	EXPECT_NE(include, UINT_MAX);
	EXPECT_NE(func, UINT_MAX);
	EXPECT_NE(body, UINT_MAX);
	EXPECT_NE(call, UINT_MAX);

	EXPECT_EQ(hlir_ast_gen(&ast, &hlir), 0);
	char out[256] = {0};
	size_t len = ast_c_print(&ast, DST_BUF(out));
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("#include <stdbool.h>")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("store_u16_le(xram, DPTR, 0x0005);")), 1);

	hlir_free(&hlir);
	ast_free(&ast);

	END;
}

TEST(hlir_ast_oom_cleanup)
{
	START;

	hlir_t hlir = {0};
	ast_t ast = {0};
	EXPECT_NE(hlir_init(&hlir), NULL);
	EXPECT_NE(ast_init(&ast), NULL);

	EXPECT_NE(t_add(&hlir, HLIR_KIND_INCLUDE, STRV("stdbool.h"), 1), UINT_MAX);
	EXPECT_NE(t_add(&hlir, HLIR_KIND_FUNCTION, STRV("recovered"), 1), UINT_MAX);

	mem_oom(1);
	EXPECT_EQ(hlir_ast_gen(&ast, &hlir), 1);
	mem_oom(0);
	EXPECT_NE(ast_get(&ast, AST_ROOT), NULL);
	EXPECT_EQ(ast_get(&ast, AST_ROOT)->kind, AST_KIND_ROOT);

	hlir_free(&hlir);
	ast_free(&ast);

	END;
}

STEST(hlir_ast)
{
	SSTART;

	RUN(hlir_ast_null_safety);
	RUN(hlir_ast_clone_and_print);
	RUN(hlir_ast_oom_cleanup);

	SEND;
}
