#include "ast_c.h"

#include <limits.h>

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

static uint t_add(ast_t *ast, tree_node_t parent, ast_kind_t kind, strv_t text)
{
	tree_node_t node = 0;
	if (ast_new(ast, &node, kind, text) == NULL) {
		return UINT_MAX;
	}
	if (ast_add(ast, parent, node)) {
		return UINT_MAX;
	}
	return node;
}

TEST(ast_c_print_null_safety)
{
	START;

	char out[16] = {0};
	EXPECT_EQ(ast_c_print(NULL, DST_BUF(out)), 0);
	ast_t zero = {0};
	EXPECT_EQ(ast_c_print(&zero, DST_BUF(out)), 0);

	END;
}

TEST(ast_c_print_passthrough)
{
	START;

	ast_t ast = {0};
	EXPECT_NE(ast_init(&ast), NULL);

	EXPECT_NE(t_add(&ast, AST_ROOT, AST_KIND_INCLUDE, STRV("stdbool.h")), UINT_MAX);
	uint func = t_add(&ast, AST_ROOT, AST_KIND_FUNCTION, STRV("recovered"));
	EXPECT_NE(func, UINT_MAX);
	uint body = t_add(&ast, func, AST_KIND_BLOCK, STRV_NULL);
	EXPECT_NE(body, UINT_MAX);
	EXPECT_NE(t_add(&ast, body, AST_KIND_DECL, STRV("uint8_t A")), UINT_MAX);

	uint cond_lhs = 0;
	uint cond_rhs = 0;
	uint cond = 0;
	EXPECT_NE(ast_new(&ast, &cond, AST_KIND_EXPR_BINARY, STRV("==")), NULL);
	EXPECT_NE(ast_new(&ast, &cond_lhs, AST_KIND_EXPR_REF, STRV("A")), NULL);
	EXPECT_NE(ast_new(&ast, &cond_rhs, AST_KIND_EXPR_REF, STRV("B")), NULL);
	EXPECT_NE(cond_lhs, UINT_MAX);
	EXPECT_NE(cond_rhs, UINT_MAX);
	EXPECT_NE(cond, UINT_MAX);
	EXPECT_EQ(ast_add(&ast, cond, cond_lhs), 0);
	EXPECT_EQ(ast_add(&ast, cond, cond_rhs), 0);

	uint if_node = t_add(&ast, body, AST_KIND_STMT_IF, STRV_NULL);
	EXPECT_NE(if_node, UINT_MAX);
	EXPECT_EQ(ast_add(&ast, if_node, cond), 0);
	uint then_block = t_add(&ast, if_node, AST_KIND_BLOCK, STRV_NULL);
	uint else_block = t_add(&ast, if_node, AST_KIND_BLOCK, STRV_NULL);
	EXPECT_NE(then_block, UINT_MAX);
	EXPECT_NE(else_block, UINT_MAX);
	EXPECT_NE(t_add(&ast, then_block, AST_KIND_STMT_RETURN, STRV_NULL), UINT_MAX);
	EXPECT_NE(t_add(&ast, else_block, AST_KIND_STMT_UNKNOWN, STRV_NULL), UINT_MAX);

	uint while_cond_lhs = 0;
	uint while_cond_rhs = 0;
	uint while_cond = 0;
	EXPECT_NE(ast_new(&ast, &while_cond, AST_KIND_EXPR_BINARY, STRV("!=")), NULL);
	EXPECT_NE(ast_new(&ast, &while_cond_lhs, AST_KIND_EXPR_REF, STRV("A")), NULL);
	EXPECT_NE(ast_new(&ast, &while_cond_rhs, AST_KIND_EXPR_REF, STRV("B")), NULL);
	EXPECT_NE(while_cond_lhs, UINT_MAX);
	EXPECT_NE(while_cond_rhs, UINT_MAX);
	EXPECT_NE(while_cond, UINT_MAX);
	EXPECT_EQ(ast_add(&ast, while_cond, while_cond_lhs), 0);
	EXPECT_EQ(ast_add(&ast, while_cond, while_cond_rhs), 0);
	uint while_node = t_add(&ast, body, AST_KIND_STMT_WHILE, STRV_NULL);
	EXPECT_NE(while_node, UINT_MAX);
	EXPECT_EQ(ast_add(&ast, while_node, while_cond), 0);
	uint while_body = t_add(&ast, while_node, AST_KIND_BLOCK, STRV_NULL);
	EXPECT_NE(while_body, UINT_MAX);
	EXPECT_NE(t_add(&ast, while_body, AST_KIND_STMT_GOTO, STRV("block1")), UINT_MAX);

	uint if_goto_cond = 0;
	EXPECT_NE(ast_new(&ast, &if_goto_cond, AST_KIND_EXPR_REF, STRV("A")), NULL);
	EXPECT_NE(if_goto_cond, UINT_MAX);
	uint if_goto = t_add(&ast, body, AST_KIND_STMT_IF_GOTO, STRV("block2"));
	EXPECT_NE(if_goto, UINT_MAX);
	EXPECT_EQ(ast_add(&ast, if_goto, if_goto_cond), 0);

	uint swap = t_add(&ast, body, AST_KIND_STMT_SWAP, STRV_NULL);
	EXPECT_NE(swap, UINT_MAX);
	uint swap_lhs = 0;
	uint swap_rhs = 0;
	EXPECT_NE(ast_new(&ast, &swap_lhs, AST_KIND_EXPR_REF, STRV("A")), NULL);
	EXPECT_NE(ast_new(&ast, &swap_rhs, AST_KIND_EXPR_REF, STRV("B")), NULL);
	EXPECT_NE(swap_lhs, UINT_MAX);
	EXPECT_NE(swap_rhs, UINT_MAX);
	EXPECT_EQ(ast_add(&ast, swap, swap_lhs), 0);
	EXPECT_EQ(ast_add(&ast, swap, swap_rhs), 0);
	EXPECT_NE(t_add(&ast, body, AST_KIND_STMT_CALL, STRV("0x1234")), UINT_MAX);
	EXPECT_NE(t_add(&ast, body, AST_KIND_STMT_UNKNOWN, STRV_NULL), UINT_MAX);

	char out[512] = {0};
	size_t len = ast_c_print(&ast, DST_BUF(out));
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("#include <stdbool.h>")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("void recovered(void)")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("uint8_t A;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("if ((A == B)) {")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("while ((A != B)) {")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("goto block2;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("swap(")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("call_0x1234();")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("/* unknown */;")), 1);

	ast_free(&ast);

	END;
}

TEST(ast_c_print_default_expr_nodes)
{
	START;

	ast_t ast = {0};
	EXPECT_NE(ast_init(&ast), NULL);

	uint func = t_add(&ast, AST_ROOT, AST_KIND_FUNCTION, STRV("recovered"));
	EXPECT_NE(func, UINT_MAX);
	uint body = t_add(&ast, func, AST_KIND_BLOCK, STRV_NULL);
	EXPECT_NE(body, UINT_MAX);

	uint lhs = 0;
	uint rhs = 0;
	uint unknown = 0;
	uint phi_lhs = 0;
	uint phi = 0;
	uint unknown_lhs = 0;
	uint assign = 0;
	uint assign_unknown = 0;
	EXPECT_NE(ast_new(&ast, &lhs, AST_KIND_DECL, STRV("lhs")), NULL);
	EXPECT_NE(ast_new(&ast, &rhs, AST_KIND_LABEL, STRV_NULL), NULL);
	EXPECT_NE(ast_new(&ast, &unknown, AST_KIND_EXPR_UNKNOWN, STRV_NULL), NULL);
	EXPECT_NE(ast_new(&ast, &phi_lhs, AST_KIND_DECL, STRV("phi")), NULL);
	EXPECT_NE(ast_new(&ast, &phi, AST_KIND_STMT_PHI, STRV_NULL), NULL);
	EXPECT_NE(ast_new(&ast, &unknown_lhs, AST_KIND_DECL, STRV("u")), NULL);
	EXPECT_NE(ast_new(&ast, &assign, AST_KIND_STMT_ASSIGN, STRV_NULL), NULL);
	EXPECT_NE(ast_new(&ast, &assign_unknown, AST_KIND_STMT_ASSIGN, STRV_NULL), NULL);
	EXPECT_NE(lhs, UINT_MAX);
	EXPECT_NE(rhs, UINT_MAX);
	EXPECT_NE(unknown, UINT_MAX);
	EXPECT_NE(phi_lhs, UINT_MAX);
	EXPECT_NE(phi, UINT_MAX);
	EXPECT_NE(unknown_lhs, UINT_MAX);
	EXPECT_NE(assign, UINT_MAX);
	EXPECT_NE(assign_unknown, UINT_MAX);
	EXPECT_EQ(ast_add(&ast, assign, lhs), 0);
	EXPECT_EQ(ast_add(&ast, assign, rhs), 0);
	EXPECT_EQ(ast_add(&ast, phi, phi_lhs), 0);
	EXPECT_EQ(ast_add(&ast, body, assign), 0);
	EXPECT_EQ(ast_add(&ast, assign_unknown, unknown_lhs), 0);
	EXPECT_EQ(ast_add(&ast, assign_unknown, unknown), 0);
	EXPECT_EQ(ast_add(&ast, body, assign_unknown), 0);
	EXPECT_EQ(ast_add(&ast, body, phi), 0);

	uint if_no_cond = 0;
	EXPECT_NE(ast_new(&ast, &if_no_cond, AST_KIND_STMT_IF, STRV_NULL), NULL);
	EXPECT_NE(if_no_cond, UINT_MAX);
	EXPECT_EQ(ast_add(&ast, body, if_no_cond), 0);

	char out[256] = {0};
	size_t len = ast_c_print(&ast, DST_BUF(out));
	EXPECT_GT(len, 0);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("lhs = ;")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("lhs")), 1);
	EXPECT_EQ(t_c_str_contains(STRVN(out, len), STRV("void recovered(void)")), 1);

	ast_free(&ast);

	END;
}

STEST(ast_c)
{
	SSTART;

	RUN(ast_c_print_null_safety);
	RUN(ast_c_print_passthrough);
	RUN(ast_c_print_default_expr_nodes);

	SEND;
}
