#ifndef LLIR_C_H
#define LLIR_C_H

#include "llir_c_ast.h"

size_t llir_c_print(const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars,
		    const llir_types_t *types, dst_t dst);

#endif
