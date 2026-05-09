#ifndef LLIR_CFLOW_H
#define LLIR_CFLOW_H

#include "llir_expr.h"
#include "llir_ssa.h"
#include "llir_vars.h"

typedef enum llir_cflow_block_kind_e {
	LLIR_CFLOW_BLOCK_LINEAR,
	LLIR_CFLOW_BLOCK_IF,
	LLIR_CFLOW_BLOCK_IF_ELSE,
	LLIR_CFLOW_BLOCK_LOOP,
	LLIR_CFLOW_BLOCK_TERMINAL,
} llir_cflow_block_kind_t;

typedef struct llir_cflow_block_s {
	uint ssa_block;
	llir_cflow_block_kind_t kind;
	uint then_block;
	uint else_block;
	uint join_block;
	uint loop_exit;
} llir_cflow_block_t;

typedef struct llir_cflow_s {
	arr_t blocks;
	alloc_t alloc;
} llir_cflow_t;

llir_cflow_t *llir_cflow_init(llir_cflow_t *cflow, uint cap, alloc_t alloc);
void llir_cflow_free(llir_cflow_t *cflow);

int llir_cflow_gen(llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr);

size_t llir_cflow_print(const llir_cflow_t *cflow, const llir_ssa_t *ssa, const llir_expr_t *expr, const llir_vars_t *vars, dst_t dst);

#endif
