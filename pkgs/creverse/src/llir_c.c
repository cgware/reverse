#include "llir_c.h"

size_t llir_c_print(const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars,
		    const llir_types_t *types, dst_t dst)
{
	llir_c_ast_t ast = {0};
	if (llir_c_ast_init(&ast) == NULL) {
		return 0; // LCOV_EXCL_LINE
	}
	if (llir_c_ast_gen(&ast, cflow, ssa, expr, vars, types) != 0) {
		return 0;
	}
	size_t len = llir_c_ast_emit(&ast, dst);
	llir_c_ast_free(&ast);
	return len;
}
