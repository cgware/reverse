#ifndef LLIR_C_AST_H
#define LLIR_C_AST_H

#include "llir_types.h"

typedef struct llir_c_ast_s {
	const llir_cflow_t *cflow;
	const llir_ssa_t *ssa;
	const llir_expr_t *expr;
	const llir_vars_t *vars;
	const llir_types_t *types;
} llir_c_ast_t;

llir_c_ast_t *llir_c_ast_init(llir_c_ast_t *ast);
void llir_c_ast_free(llir_c_ast_t *ast);

int llir_c_ast_gen(llir_c_ast_t *ast, const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars,
		   const llir_types_t *types);

size_t llir_c_ast_emit(const llir_c_ast_t *ast, dst_t dst);
size_t llir_c_ast_print(const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars,
			const llir_types_t *types, dst_t dst);

#endif
