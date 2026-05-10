#include "hlir_ast.h"

static int hlir_ast_attach(ast_t *ast, tree_node_t parent, const hlir_node_t *src, tree_node_t *out)
{
	if (ast == NULL || src == NULL || out == NULL) {
		return 1;
	}

	ast_node_t *node = ast_new(ast, out, (ast_kind_t)src->kind, src->text.data != NULL ? STRVS(src->text) : STRV_NULL);
	if (node == NULL) {
		return 1;
	}

	if (ast_add(ast, parent, *out)) {
		ast_remove(ast, *out);
		return 1;
	}

	return 0;
}

int hlir_ast_gen(ast_t *ast, const hlir_t *hlir)
{
	if (ast == NULL || hlir == NULL || ast->data == NULL || ast->cnt == 0) {
		return 1;
	}

	ast_reset(ast);

	tree_node_t stack[HLIR_MAX_DEPTH] = {0};
	stack[0] = AST_ROOT;

	uint i = 0;
	const hlir_node_t *src;
	arr_foreach(hlir, i, src)
	{
		if (src == NULL || !src->used) {
			continue;
		}

		if (src->depth == 0) {
			stack[0] = AST_ROOT;
			continue;
		}

		if (src->depth >= HLIR_MAX_DEPTH) {
			ast_reset(ast); // LCOV_EXCL_LINE
			return 1; // LCOV_EXCL_LINE
		}

		tree_node_t parent = stack[src->depth - 1];
		tree_node_t dst = 0;
		if (hlir_ast_attach(ast, parent, src, &dst)) {
			ast_reset(ast);
			return 1;
		}

		stack[src->depth] = dst;
		for (uint j = src->depth + 1; j < HLIR_MAX_DEPTH; j++) {
			stack[j] = 0;
		}
	}

	return 0;
}
