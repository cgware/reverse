#ifndef LLIR_AST_H
#define LLIR_AST_H

#include "ast.h"
#include "llir_types.h"

int llir_ast_gen(ast_t *ast, const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars,
		 const llir_types_t *types);

#endif
