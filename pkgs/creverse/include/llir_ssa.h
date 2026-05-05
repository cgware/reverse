#ifndef LLIR_SSA_H
#define LLIR_SSA_H

#include "llir.h"

typedef struct llir_ssa_phi_arg_s {
	uint pred;
	uint ver;
} llir_ssa_phi_arg_t;

typedef struct llir_ssa_phi_s {
	asmc_reg_type_t reg;
	uint ver;
	arr_t args;
} llir_ssa_phi_t;

typedef struct llir_ssa_block_s {
	uint start;
	uint end;
	arr_t preds;
	arr_t succs;
	arr_t dom_children;
	arr_t dom_frontier;
	arr_t phis;
	uint idom;
	byte reachable;
} llir_ssa_block_t;

typedef struct llir_ssa_inst_s {
	llir_op_t op;
	uint dst_ver;
	uint src_ver;
	uint cmp_ver;
	uint dst_out_ver;
	uint src_out_ver;
} llir_ssa_inst_t;

typedef struct llir_ssa_s {
	arr_t blocks;
	arr_t ops;
	alloc_t alloc;
} llir_ssa_t;

llir_ssa_t *llir_ssa_init(llir_ssa_t *ssa, alloc_t alloc);
void llir_ssa_free(llir_ssa_t *ssa);

int llir_ssa_gen(llir_ssa_t *ssa, const llir_t *llir);

size_t llir_ssa_print(const llir_ssa_t *ssa, dst_t dst);

#endif
