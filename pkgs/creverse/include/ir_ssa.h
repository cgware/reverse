#ifndef IR_SSA_H
#define IR_SSA_H

#include "ir.h"

typedef struct ir_ssa_phi_arg_s {
	uint pred;
	uint ver;
} ir_ssa_phi_arg_t;

typedef struct ir_ssa_phi_s {
	asmc_reg_type_t reg;
	uint ver;
	arr_t args;
} ir_ssa_phi_t;

typedef struct ir_ssa_block_s {
	uint start;
	uint end;
	arr_t preds;
	arr_t succs;
	arr_t dom_children;
	arr_t dom_frontier;
	arr_t phis;
	uint idom;
	byte reachable;
} ir_ssa_block_t;

typedef struct ir_ssa_inst_s {
	ir_op_t op;
	uint dst_ver;
	uint src_ver;
	uint cmp_ver;
	uint dst_out_ver;
	uint src_out_ver;
} ir_ssa_inst_t;

typedef struct ir_ssa_s {
	arr_t blocks;
	arr_t ops;
	alloc_t alloc;
} ir_ssa_t;

ir_ssa_t *ir_ssa_init(ir_ssa_t *ssa, alloc_t alloc);
void ir_ssa_free(ir_ssa_t *ssa);

int ir_ssa_gen(ir_ssa_t *ssa, const ir_t *ir);

size_t ir_ssa_print(const ir_ssa_t *ssa, dst_t dst);

#endif
