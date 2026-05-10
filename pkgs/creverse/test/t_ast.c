#include "ast.h"

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

TEST(ast_kind_name_all)
{
	START;

	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_ROOT), "root"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_INCLUDE), "include"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_FUNCTION), "function"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_BLOCK), "block"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_DECL), "decl"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_LABEL), "label"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_PHI), "phi"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_ASSIGN), "assign"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_BIN_ASSIGN), "bin_assign"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_IF_GOTO), "if_goto"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_IF), "if"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_WHILE), "while"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_GOTO), "goto"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_SWAP), "swap"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_CALL), "call"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_RETURN), "return"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_STMT_UNKNOWN), "unknown"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_EXPR_CONST), "const"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_EXPR_REF), "ref"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_EXPR_UNARY), "unary"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_EXPR_BINARY), "binary"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name(AST_KIND_EXPR_UNKNOWN), "expr_unknown"), 1);
	EXPECT_EQ(t_c_str_eq(ast_kind_name((ast_kind_t)-1), "unknown"), 1);

	END;
}

TEST(ast_api_null_safety)
{
	START;

	ast_t ast = {0};
	ast_t bad = {0};
	tree_node_t node = 0;

	EXPECT_EQ(ast_init(NULL), NULL);
	EXPECT_EQ(ast_new(NULL, NULL, AST_KIND_ROOT, STRV_NULL), NULL);
	EXPECT_EQ(ast_set_kind(NULL, 0, AST_KIND_ROOT), 1);
	EXPECT_EQ(ast_set_text(NULL, 0, STRV("x")), 1);
	EXPECT_EQ(ast_setf(NULL, 0, "%s", "x"), 1);
	EXPECT_EQ(ast_setf(&ast, 0, NULL), 1);
	EXPECT_EQ(ast_add(NULL, 0, 0), 1);
	EXPECT_EQ(ast_app(NULL, 0, 0), 1);
	EXPECT_EQ(ast_remove(NULL, 0), 1);
	EXPECT_EQ(ast_get(NULL, 0), NULL);
	EXPECT_EQ(ast_get_child(NULL, 0, NULL), NULL);
	EXPECT_EQ(ast_get_next(NULL, 0, NULL), NULL);
	EXPECT_EQ(ast_print(NULL, DST_NONE()), 0);
	ast_t zero = {0};
	EXPECT_EQ(ast_print(&zero, DST_NONE()), 0);
	ast_free(NULL);
	ast_reset(NULL);

	mem_oom(1);
	EXPECT_EQ(ast_init(&bad), NULL);
	mem_oom(0);

	mem_oom(2);
	EXPECT_EQ(ast_init(&bad), NULL);
	mem_oom(0);

	EXPECT_NE(ast_init(&ast), NULL);
	EXPECT_NE(ast_get(&ast, AST_ROOT), NULL);
	EXPECT_EQ(ast_get(&ast, AST_ROOT)->kind, AST_KIND_ROOT);
	EXPECT_EQ(ast_set_text(&ast, AST_ROOT, STRV_NULL), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(ast_new(&ast, NULL, AST_KIND_INCLUDE, STRV("x")), NULL);
	EXPECT_EQ(ast_add(&ast, ast.cnt, ast.cnt), 1);
	EXPECT_EQ(ast_app(&ast, ast.cnt, ast.cnt), 1);
	EXPECT_EQ(ast_remove(&ast, ast.cnt), 1);
	EXPECT_EQ(ast_get(&ast, ast.cnt), NULL);
	EXPECT_EQ(ast_get_child(&ast, ast.cnt, NULL), NULL);
	EXPECT_EQ(ast_get_next(&ast, ast.cnt, NULL), NULL);
	log_set_quiet(0, 0);

	mem_oom(1);
	EXPECT_EQ(ast_new(&ast, &node, AST_KIND_INCLUDE, STRV("x")), NULL);
	mem_oom(0);

	mem_oom(2);
	EXPECT_EQ(ast_new(&ast, &node, AST_KIND_INCLUDE, STRV("x")), NULL);
	mem_oom(0);

	mem_oom(1);
	EXPECT_EQ(ast_set_text(&ast, AST_ROOT, STRV("x")), 1);
	mem_oom(0);

	mem_oom(1);
	EXPECT_EQ(ast_setf(&ast, AST_ROOT, "%s", "x"), 1);
	mem_oom(0);

	ast_free(&ast);

	END;
}

TEST(ast_construct_tree)
{
	START;

	ast_t ast = {0};
	EXPECT_NE(ast_init(&ast), NULL);

	tree_node_t include = 0;
	tree_node_t func = 0;
	tree_node_t body = 0;
	tree_node_t decl = 0;
	tree_node_t label = 0;
	tree_node_t expr = 0;

	EXPECT_NE(ast_new(&ast, &include, AST_KIND_INCLUDE, STRV("stdbool.h")), NULL);
	EXPECT_NE(ast_new(&ast, &func, AST_KIND_FUNCTION, STRV("recovered")), NULL);
	EXPECT_NE(ast_new(&ast, &body, AST_KIND_BLOCK, STRV_NULL), NULL);
	EXPECT_NE(ast_new(&ast, &decl, AST_KIND_DECL, STRV("uint8_t A")), NULL);
	EXPECT_NE(ast_new(&ast, &label, AST_KIND_LABEL, STRV("block1")), NULL);
	EXPECT_NE(ast_new(&ast, &expr, AST_KIND_EXPR_CONST, STRV("0x01")), NULL);

	EXPECT_EQ(ast_add(&ast, AST_ROOT, include), 0);
	EXPECT_EQ(ast_add(&ast, AST_ROOT, func), 0);
	EXPECT_EQ(ast_add(&ast, func, body), 0);
	EXPECT_EQ(ast_add(&ast, body, decl), 0);
	EXPECT_EQ(ast_add(&ast, body, label), 0);
	EXPECT_EQ(ast_add(&ast, body, expr), 0);

	tree_node_t next = 0;
	EXPECT_EQ(ast_get_child(&ast, AST_ROOT, &next)->kind, AST_KIND_INCLUDE);
	EXPECT_EQ(next, include);
	EXPECT_NE(ast_get_next(&ast, include, &next), NULL);
	EXPECT_EQ(next, func);

	EXPECT_EQ(ast_get_child(&ast, func, &next)->kind, AST_KIND_BLOCK);
	EXPECT_EQ(next, body);

	char out[256] = {0};
	size_t len = ast_print(&ast, DST_BUF(out));
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("root")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("include stdbool.h")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("function recovered")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("decl uint8_t A")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("label block1")), 1);

	EXPECT_EQ(ast_remove(&ast, label), 0);
	EXPECT_NE(ast_get_next(&ast, decl, &next), NULL);
	EXPECT_EQ(next, expr);

	ast_reset(&ast);
	EXPECT_NE(ast_get(&ast, AST_ROOT), NULL);
	EXPECT_EQ(ast_get(&ast, AST_ROOT)->kind, AST_KIND_ROOT);
	EXPECT_EQ(ast_get_child(&ast, AST_ROOT, &next), NULL);

	ast_free(&ast);

	END;
}

TEST(ast_set_text_and_print)
{
	START;

	ast_t ast = {0};
	EXPECT_NE(ast_init(&ast), NULL);

	tree_node_t node = 0;
	EXPECT_NE(ast_new(&ast, &node, AST_KIND_INCLUDE, STRV("stdbool.h")), NULL);
	EXPECT_EQ(ast_add(&ast, AST_ROOT, node), 0);
	EXPECT_EQ(ast_set_kind(&ast, node, AST_KIND_INCLUDE), 0);
	EXPECT_EQ(ast_set_text(&ast, node, STRV("stdint.h")), 0);
	EXPECT_EQ(ast_setf(&ast, node, "%s", "stdbool.h"), 0);

	char out[128] = {0};
	size_t len = ast_print(&ast, DST_BUF(out));
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("include stdbool.h")), 1);

	ast_free(&ast);

	END;
}

STEST(ast)
{
	SSTART;

	RUN(ast_api_null_safety);
	RUN(ast_construct_tree);
	RUN(ast_set_text_and_print);
	RUN(ast_kind_name_all);

	SEND;
}
