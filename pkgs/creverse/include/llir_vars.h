#ifndef LLIR_VARS_H
#define LLIR_VARS_H

#include "llir_expr.h"

typedef struct llir_vars_var_s {
	llir_reg_type_t reg;
	u8 size;
	uint first_ver;
	uint last_ver;
} llir_vars_var_t;

typedef struct llir_vars_s {
	arr_t vars;
	alloc_t alloc;
} llir_vars_t;

llir_vars_t *llir_vars_init(llir_vars_t *vars, uint cap, alloc_t alloc);
void llir_vars_free(llir_vars_t *vars);

int llir_vars_gen(llir_vars_t *vars, const llir_expr_t *expr);
int llir_vars_cleanup(llir_vars_t *vars, const llir_expr_t *expr);

size_t llir_vars_print(const llir_vars_t *vars, const llir_expr_t *expr, dst_t dst);

#endif
